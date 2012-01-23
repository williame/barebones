#ifndef __G3D_HPP__
#define __G3D_HPP__

#include "main.hpp"
#include "../external/ogl-math/glm/glm.hpp"
#include "../external/ogl-math/glm/gtc/type_ptr.hpp"

class binary_reader_t;

class g3d_t: private main_t::file_io_t {
public:
	struct loaded_t {
		virtual void on_g3d_loaded(g3d_t& g3d,bool ok,intptr_t data) = 0; // throw error if upset
	};
	g3d_t(main_t& main,const std::string& filename,loaded_t* observer=NULL,intptr_t data=0);
	main_t& main;
	const std::string filename;
	void draw(float time,const glm::mat4& projection,const glm::mat4& modelview,const glm::vec3& light_0,bool cycles,const glm::vec4& colour = glm::vec4(1,1,1,1));
	void bounds(glm::vec3& min,glm::vec3& max);
	bool is_ready() const;
private:
	struct mesh_t;
	friend struct mesh_t;
	enum { LOAD_G3D };
	void on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data);
	void on_ready(mesh_t* mesh);
	typedef std::vector<mesh_t*> meshes_t;
	meshes_t meshes;
	loaded_t* observer;
	intptr_t observer_data;
};

class binary_reader_t {
public:
	binary_reader_t(const std::string& d): data(d), ofs(0) {}
	inline uint8_t byte() { return _r<uint8_t>(); }
	inline uint16_t uint16() { return _r<uint16_t>(); };
	inline uint32_t uint32() { return _r<uint32_t>(); };
	inline float float32() { return _r<float>(); }
	inline void skip(size_t bytes) { ofs += bytes; }
	inline void read(void* dest,size_t bytes) { for(char* d = reinterpret_cast<char*>(dest); bytes--; d++) *d = data.at(ofs++); }
	template<int N> std::string fixed_str() { ofs += N; return data.substr(ofs-N,N); }
private:
	template<typename T> T _r() { T v; read(&v,sizeof(T)); return v; }
	const std::string& data;
	size_t ofs;
};

#endif//__G3D_HPP__
