#include "g3d.hpp"
#include <iostream>
#include <limits>

struct g3d_t::mesh_t: private main_t::texture_load_t {
public:
	mesh_t(g3d_t& g3d,binary_reader_t& in,char ver);
	virtual ~mesh_t();
	void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light_0,const glm::vec4& colour);
	bool is_ready() const { return i_vbo && (!(textures&1) || texture); }
	g3d_t& g3d;
	std::string name;
	uint32_t frame_count, vertex_count, index_count, textures, tex_frame_count;
	GLfloat* vn_data;
	GLfloat* t_data;
	GLushort* i_data;
	GLuint* vn_vbo; // per frame
	GLuint* t_vbo; // per tex_frame
	GLuint i_vbo;
	GLuint texture, program,
		uniform_mvp_matrix, uniform_normal_matrix, uniform_light_0, uniform_colour,
		attrib_vertex_0, attrib_normal_0,
		attrib_vertex_1, attrib_normal_1, uniform_lerp,
		attrib_tex;
	glm::vec3 min, max;
private:
	void on_texture_loaded(const std::string& name,GLuint handle,intptr_t data);
	enum { LOAD_TEXTURE };
};

g3d_t::g3d_t(main_t& m,const std::string& fn,loaded_t* o,intptr_t od): main(m), filename(fn),
	observer(o), observer_data(od) {
	main.read_file(filename,this,LOAD_G3D);
}

void g3d_t::on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data) {
	try {
		if(!ok || !bytes.size())
			data_error("could not load");
		if(LOAD_G3D == data) {
			binary_reader_t in(bytes);
			const uint32_t ver = in.uint32();
			// note the endian here is little endian
			if(((ver&0xff)!='G')||(((ver>>8)&0xff)!='3')||(((ver>>16)&0xff)!='D'))
				data_error("(" << std::hex << ver << ") is not a G3D model");
			switch(ver>>24) {
			//case 3: {
			//} break;
			case 4: {
				const uint16_t mesh_count = in.uint16();
				if(!mesh_count) data_error("has no meshes");
				if(in.byte()) data_error("not a G3D mtMorphMesh");
				for(int16_t i=0; i<mesh_count; i++)
					meshes.push_back(new mesh_t(*this,in,ver>>24));
			} break;
			default: data_error("not a supported G3D model version (" << (ver&0xff) << ")");
			}
		} else
			data_error("stray io " << name << ',' << data);
	} catch(std::exception& e) {
		std::cerr << "ERROR loading G3D " << filename << ": " << e.what() << std::endl;
		meshes.clear();
		ok = false;
	}
}

g3d_t::mesh_t::mesh_t(g3d_t& g,binary_reader_t& in,char ver):
	g3d(g),
	vn_data(NULL), t_data(NULL), i_data(NULL), vn_vbo(NULL), t_vbo(NULL), i_vbo(0),
	texture(0), program(0),
	min(FLT_MAX/2,FLT_MAX/2,FLT_MAX/2), max(-FLT_MAX/2,-FLT_MAX/2,-FLT_MAX/2) {
	if(ver==4) {
		name = std::string(in.fixed_str<64>().c_str());
		frame_count = in.uint32(); if(!frame_count) data_error(name << " has no frames");
		vertex_count = in.uint32(); if(!vertex_count) data_error(name << " has no vertices");
		index_count = in.uint32(); if(!index_count) data_error(name << " has no indices");
		if(index_count%3) data_error(name << " bad number of indices: " << index_count);
		in.skip(9*4);
		textures = in.uint32();
		for(int t=0; t<5; t++)
			if((1<<t)&textures) {
				const std::string path = std::string(in.fixed_str<64>().c_str());
				if(t==0) // diffuse?
					g3d.main.load_texture(g3d.main.relpath(g3d.filename,path),this,LOAD_TEXTURE);
			}
		tex_frame_count = textures?1:0;
	}
	const size_t vn_size = vertex_count*frame_count*6;
	vn_data = new GLfloat[vn_size];
	for(int pass=0; pass<2; pass++) //0==vertices,1==normals
		for(uint32_t f=0; f<frame_count; f++)
			for(uint32_t v=0; v<vertex_count; v++)
				for(int j=0; j<3; j++) {
					const size_t slot = f*vertex_count*6 + v*6 + pass*3 + j;
					assert(slot < vn_size);
					vn_data[slot] = in.float32();
					min[j] = std::min(vn_data[slot],min[j]);
					max[j] = std::max(vn_data[slot],max[j]);
				}
	vn_vbo = new GLuint[frame_count];
	glGenBuffers(frame_count,vn_vbo);
	glCheck();
	for(uint32_t f=0; f<frame_count; f++) {
		glBindBuffer(GL_ARRAY_BUFFER,vn_vbo[f]);
		glBufferData(GL_ARRAY_BUFFER,vertex_count*6*sizeof(GLfloat),vn_data+f*vertex_count*6,GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER,0);
		glCheck();
	}
	const size_t texture_size = textures?tex_frame_count*vertex_count*2:0;
	t_data = new GLfloat[texture_size];
	for(uint32_t f=0; f<tex_frame_count; f++)
		for(uint32_t v=0; v<vertex_count; v++) {
			size_t slot = f*vertex_count*2+v*2;
			assert(slot < texture_size-1);
			t_data[slot] = in.float32();
			t_data[slot+1] = 1.-in.float32(); // invert Y
		}
	t_vbo = new GLuint[tex_frame_count];
	glGenBuffers(tex_frame_count,t_vbo);
	glCheck();
	for(uint32_t f=0; f<tex_frame_count; f++) {
		glBindBuffer(GL_ARRAY_BUFFER,t_vbo[f]);
		glBufferData(GL_ARRAY_BUFFER,vertex_count*2*sizeof(GLfloat),t_data+f*vertex_count*2,GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER,0);
		glCheck();
	}
	i_data = new GLushort[index_count];
	for(uint32_t i=0; i<index_count; i++) {
		i_data[i] = in.uint32();
		if(i_data[i] >= vertex_count)
			data_error("index[" << i << "]=" << i_data[i] << " out of bounds (" << vertex_count << ')');
	}
	glGenBuffers(1,&i_vbo);
	glCheck();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i_vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,index_count*sizeof(GLushort),i_data,GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
	glCheck();
	if(1 == frame_count) {
		program = g3d.main.get_shared_program("g3d_single_frame");
		graphics_assert(program && "g3d_single_frame"); // provided by game adaptation
	} else {
		program = g3d.main.get_shared_program("g3d_multi_frame");
		graphics_assert(program && "g3d_multi_frame"); // provided by game adaptation
		uniform_lerp = g3d.main.get_uniform_loc(program,"LERP",GL_FLOAT);
		attrib_vertex_1 = g3d.main.get_attribute_loc(program,"VERTEX_1",GL_FLOAT_VEC3);
		attrib_normal_1 = g3d.main.get_attribute_loc(program,"NORMAL_1",GL_FLOAT_VEC3);
		glEnableVertexAttribArray(attrib_vertex_1);
		glEnableVertexAttribArray(attrib_normal_1);
	}
	uniform_mvp_matrix = g3d.main.get_uniform_loc(program,"MVP_MATRIX",GL_FLOAT_MAT4);
	uniform_normal_matrix = g3d.main.get_uniform_loc(program,"NORMAL_MATRIX",GL_FLOAT_MAT3);
	uniform_light_0 = g3d.main.get_uniform_loc(program,"LIGHT_0",GL_FLOAT_VEC3);
	uniform_colour = g3d.main.get_uniform_loc(program,"COLOUR",GL_FLOAT_VEC4);
	attrib_vertex_0 = g3d.main.get_attribute_loc(program,"VERTEX_0",GL_FLOAT_VEC3);
	attrib_normal_0 = g3d.main.get_attribute_loc(program,"NORMAL_0",GL_FLOAT_VEC3);
	attrib_tex = g3d.main.get_attribute_loc(program,"TEX_COORD_0",GL_FLOAT_VEC2);
	glUseProgram(program);
	glCheck();
	glUniform1i(g3d.main.get_uniform_loc(program,"TEX_UNIT_0"),0);
	glEnableVertexAttribArray(attrib_vertex_0);
	glEnableVertexAttribArray(attrib_normal_0);
	if(!(textures&1))
		g3d.on_ready(this);
}

g3d_t::mesh_t::~mesh_t() {
	delete[] vn_data;
	delete[] t_data;
	if(vn_vbo) glDeleteBuffers(frame_count,vn_vbo);
	delete[] vn_vbo;
	if(t_vbo) glDeleteBuffers(tex_frame_count,t_vbo);
	delete[] t_vbo;
	delete[] i_data;
	if(i_vbo) glDeleteBuffers(1,&i_vbo);
}


void g3d_t::mesh_t::draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light_0,const glm::vec4& colour) {
	if(!i_vbo || ((textures&1) && !texture)) {
		std::cerr << "cannot draw " << g3d.filename << ':' << name << " because it is not initialized (" << i_vbo << ',' << textures << ',' << texture << ')' << std::endl;
		return;
	}
	time = std::min(std::max(time,0.0f),1.0f) * (float)frame_count;
	const size_t frame_0 = (size_t)time % frame_count;
	glUseProgram(program);
	glCheck();
	glUniform4fv(uniform_colour,1,glm::value_ptr(const_cast<glm::vec4&>(colour)));
	glUniform3fv(uniform_light_0,1,glm::value_ptr(const_cast<glm::vec3&>(light_0)));
	glUniformMatrix4fv(uniform_mvp_matrix,1,false,glm::value_ptr(projection*modelview));
	glUniformMatrix3fv(uniform_normal_matrix,1,false,glm::value_ptr(glm::inverse(glm::mat3(modelview))));
	glCheck();
	const GLsizei stride = 6*sizeof(GLfloat);
	glBindBuffer(GL_ARRAY_BUFFER,vn_vbo[frame_0]);	
	glVertexAttribPointer(attrib_vertex_0,3,GL_FLOAT,GL_FALSE,stride,(void*)(0));
	glVertexAttribPointer(attrib_normal_0,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(GLfloat)));
	glCheck();
	if(frame_count > 1) {
		const size_t frame_1 = (frame_0+1) % frame_count;
		const float lerp = fmod(time,1);
		glUniform1f(uniform_lerp,lerp);
		glBindBuffer(GL_ARRAY_BUFFER,vn_vbo[frame_1]);	
		glVertexAttribPointer(attrib_vertex_1,3,GL_FLOAT,GL_FALSE,stride,(void*)(0));
		glVertexAttribPointer(attrib_normal_1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(GLfloat)));
		glCheck();
	}
	glBindTexture(GL_TEXTURE_2D,texture);
	if((textures&1) && texture) {
		const size_t tex_frame = (size_t)(std::min(std::max(time,0.0f),1.0f) * (float)tex_frame_count) % tex_frame_count;
		glBindBuffer(GL_ARRAY_BUFFER,t_vbo[tex_frame]);
		glEnableVertexAttribArray(attrib_tex);
		glVertexAttribPointer(attrib_tex,2,GL_FLOAT,GL_FALSE,2*sizeof(GLfloat),(void*)(0));
		glCheck();
	} else
		glDisableVertexAttribArray(attrib_tex);
	graphics_assert(i_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,i_vbo);
	glDrawElements(GL_TRIANGLES,index_count,GL_UNSIGNED_SHORT,(void*)(0));
	glCheck();
}

void g3d_t::mesh_t::on_texture_loaded(const std::string& name,GLuint handle,intptr_t data) {
	if(!handle || (data != LOAD_TEXTURE))
		data_error(g3d.filename << ':' << this->name << " could not load " << name << ',' << data);
	texture = handle;
	g3d.on_ready(this);
}

void g3d_t::draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light_0,const glm::vec4& colour) {
	for(meshes_t::iterator m=meshes.begin(); m!=meshes.end(); m++)
		(*m)->draw(time,projection,modelview,light_0,colour);
}

void g3d_t::bounds(glm::vec3& min,glm::vec3& max) {
	min = glm::vec3(FLT_MAX,FLT_MAX,FLT_MAX);
	max = glm::vec3(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	for(meshes_t::const_iterator m=meshes.begin(); m!=meshes.end(); m++) {
		min = glm::vec3(std::min(min.x,(*m)->min.x),std::min(min.y,(*m)->min.y),std::min(min.z,(*m)->min.z));
		max = glm::vec3(std::max(max.x,(*m)->max.x),std::max(max.y,(*m)->max.y),std::max(max.z,(*m)->max.z));
	}
}

bool g3d_t::is_ready() const {
	for(meshes_t::const_iterator m=meshes.begin(); m!=meshes.end(); m++)
		if(!(*m)->is_ready())
			return false;
	return true;
}

void g3d_t::on_ready(mesh_t* mesh) {
	if(is_ready() && observer)
		observer->on_g3d_loaded(*this,true,observer_data);
}

