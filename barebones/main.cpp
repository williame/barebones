#include "main.hpp"
#include "build_info.hpp"
#include <memory>
#include <map>
#include <iostream>

#include "../external/SOIL/SOIL.h"

#ifdef __native_client__
	#include "ppapi/cpp/instance.h"
	#include "ppapi/cpp/module.h"
	#include "ppapi/cpp/input_event.h"
	#include "ppapi/cpp/rect.h"
	#include "ppapi/gles2/gl2ext_ppapi.h"
	#include "ppapi/cpp/var.h"
	#include "ppapi/cpp/url_loader.h"
	#include "ppapi/cpp/url_request_info.h"
	#include "ppapi/cpp/url_response_info.h"
	#include "ppapi/cpp/completion_callback.h"
	#include "ppapi/cpp/graphics_3d_client.h"
	#include "ppapi/cpp/graphics_3d.h"
#else
	#include <SDL.h>
#endif

namespace {
	struct _file_io_impl_t;
	struct _texture_t;
} // anon namespace

struct main_t::_pimpl_t {
	_pimpl_t(main_t& main,void* instance);
	main_t& main;
	typedef std::vector<callback_t*> callbacks_t;
	callbacks_t callbacks;
	void callback();
	typedef std::vector<_file_io_impl_t*> file_io_impls_t;
	file_io_impls_t file_io_impls;
	typedef std::map<std::string,_texture_t*> textures_t;
	textures_t textures;
	typedef std::map<std::string,GLuint> shared_programs_t;
	shared_programs_t shared_programs;
#ifdef __native_client__
	pp::Instance* instance;
#endif
};

namespace {
	struct _file_io_impl_t: public main_t::callback_t {
		_file_io_impl_t(main_t::_pimpl_t& p,const std::string& n,main_t::file_io_t* cb,intptr_t d): 
			pimpl(p), name(n), callback(cb), data(d), ok(false), cancelled(false)
	#ifdef __native_client__
			, nc_url_loader(p.instance), nc_url_info(p.instance) {
			nc_url_info.SetURL(pp::Var(name));
			if(PP_OK_COMPLETIONPENDING != nc_url_loader.Open(nc_url_info,pp::CompletionCallback(nc_url_open,this)))
				nc_url_do_read(); //### this flow untested; chrome has always returned PP_OK_COMPLETIONPENDING in testing
		}
	#else
		{
			if(FILE* file = fopen(name.c_str(),"r")) {
				fseek(file,0,SEEK_END);
				bytes.resize(ftell(file));
				fseek(file,0,SEEK_SET);
				if(bytes.size() == fread(&bytes.at(0),1,bytes.size(),file))
					ok = true;
				fclose(file);
			}
			fire();
		}
	#endif
		main_t::_pimpl_t& pimpl;
		const std::string name;
		main_t::file_io_t* const callback;
		const intptr_t data;
		bool ok, cancelled;
		std::string bytes;
		void on_fire() {
			if(!cancelled)
				callback->on_io(name,ok,bytes,data);
			remove();
		}
		void cancel() {
			remove();
			cancelled = true;
		}
		void fire() {
			pimpl.main.add_callback(this);
		}
		void remove() {
			main_t::_pimpl_t::file_io_impls_t::iterator i = std::find(pimpl.file_io_impls.begin(),pimpl.file_io_impls.end(),this);
			if(i != pimpl.file_io_impls.end())
				pimpl.file_io_impls.erase(i);
		}
	#ifdef __native_client__
		pp::URLLoader nc_url_loader;
		pp::URLRequestInfo nc_url_info;
		size_t nc_url_ofs;
		static void nc_url_open(void* ptr,int32_t code) {
			_file_io_impl_t* self = static_cast<_file_io_impl_t*>(ptr);
			if(code || self->nc_url_loader.GetResponseInfo().is_null() || self->nc_url_loader.GetResponseInfo().GetStatusCode() != 200)
				self->fire();
			else {
				self->nc_url_ofs = 0;
				self->nc_url_do_read();
			}
		}
		static void nc_url_read(void* ptr,int32_t code) {
			_file_io_impl_t* self = static_cast<_file_io_impl_t*>(ptr);
			if(code > 0) {
				self->nc_url_ofs += code;
				self->nc_url_do_read();
				return;
			} else if(code == 0) {
				self->bytes.resize(self->nc_url_ofs);
				self->ok = true;
			}
			self->fire();
		}
		void nc_url_do_read() {
			enum { bytes_to_read = 1024*4 };
			int result;
			for(;;) {
				bytes.resize(nc_url_ofs+bytes_to_read);
				result = nc_url_loader.ReadResponseBody(&bytes.at(nc_url_ofs),bytes_to_read,pp::CompletionCallback(nc_url_read,this));
				//std::cout << "nc_url_do_read(" << path << ',' << nc_url_ofs << ")=" << result << std::endl;
				if(result > 0)
					nc_url_ofs += result;
				else if(result == PP_OK_COMPLETIONPENDING)
					return;
				else if(result == PP_OK) {
					bytes.resize(nc_url_ofs);
					ok = true;
					break;
				} else
					break;
			}
			fire();	
		}
	#endif
	};
	
	struct _texture_t: public main_t::file_io_t, public main_t::callback_t {
		_texture_t(main_t& m,const std::string& fn): main(m), filename(fn), handle(0), loaded(false) {
			main.read_file(filename,this,0);
		}
		void on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data) {
			loaded = true;
			if(ok) {
				handle = SOIL_load_OGL_texture_from_memory(
					reinterpret_cast<const unsigned char*>(bytes.c_str()),bytes.size(),
					SOIL_LOAD_AUTO,
					SOIL_CREATE_NEW_ID,
					SOIL_FLAG_POWER_OF_TWO|SOIL_FLAG_MIPMAPS);
			}
			if(queue.size())
				main.add_callback(this);
		}
		void on_fire() {
			queue_t q(queue); // copy for reentry
			queue.clear();
			for(queue_t::iterator i=q.begin(); i!=q.end(); i++)
				i->callback->on_texture_loaded(filename,handle,i->data);
		}
		void add(main_t::texture_load_t* callback,intptr_t data) {
			if(loaded && !queue.size())
				main.add_callback(this);
			const waiting_t w = {callback,data};
			queue.push_back(w);
		}
		main_t& main;
		const std::string filename;
		GLuint handle;
		bool loaded;
		struct waiting_t {
			main_t::texture_load_t* callback;
			intptr_t data;
			bool operator==(const waiting_t& rhs) const {
				return callback==rhs.callback && data==rhs.data;
			}
		};
		typedef std::vector<waiting_t> queue_t;
		queue_t queue;
	};
} // anon namespace

void main_t::_pimpl_t::callback() {
	if(callbacks.size()) {
		callbacks_t cb(callbacks); // from copy
		callbacks.clear();
		for(callbacks_t::iterator i=cb.begin(); i!=cb.end(); i++)
			(*i)->on_fire();
	}
}

main_t::main_t(void* platform_ptr): width(0), height(0), _pimpl(new _pimpl_t(*this,platform_ptr)) {
	glCheck();
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
#ifndef __native_client__
	glEnable(GL_TEXTURE_2D);
#endif
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); //(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
	glFrontFace(GL_CCW);
	glClearColor(0,0,0,1);
	glCheck();
}

main_t::~main_t() {
	delete _pimpl;
}

static void glsl_log(GLuint obj,const std::string& src) {
	int len = 0;
	char log[4096];
	GLint ok = GL_FALSE;
	glCheck();
	if(glIsShader(obj)) {
		glGetShaderiv(obj,GL_COMPILE_STATUS,&ok);
		glGetShaderInfoLog(obj,sizeof(log),&len,log);
	}  else if(glIsProgram(obj)) {
		glGetProgramiv(obj,GL_LINK_STATUS,&ok);		
		glGetProgramInfoLog(obj,sizeof(log),&len,log);
	} else
		len = snprintf(log,sizeof(log),"%u is not a program nor shader",(unsigned)obj);
	glCheck();
	std::string ret(log,len);
	while(ret.size() && ret.at(ret.size()-1) <= 32)
		ret.resize(ret.size()-1);
	if(ret.size())
		fprintf(stderr,"%s %s: %s\nsource>\n%s\n",
			glIsShader(obj)?"shader":glIsProgram(obj)?"program":"invalid handle",
			ok?"compiled with warnings":"failed to compile",
			ret.c_str(),src.c_str());
		fprintf(stderr,"source>\n%s\n",src.c_str());
	if(!ok) {
		if(!ret.size())
			fprintf(stderr,"source>\n%s\n",src.c_str());
		graphics_error(ret.c_str());
	}
}

GLuint main_t::create_program(const char* vertex,const char* fragment) {
#ifdef __native_client__
	const std::string precision("precision lowp float;\n");
#else
	const std::string precision;
#endif
	const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	graphics_assert(vs);
	glCheck();
	const std::string vsrc = precision+vertex;
	const GLchar* vvsrc = vsrc.c_str();
	glShaderSource(vs,1,&vvsrc,NULL);
	glCompileShader(vs);
	glCheck(vsrc.c_str());
	glsl_log(vs,vsrc);
	const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glCheck();
	const std::string fsrc = precision+fragment;
	const GLchar* ffsrc = fsrc.c_str();
	glShaderSource(fs,1,&ffsrc,NULL);
	glCompileShader(fs);
	glCheck(fsrc.c_str());
	glsl_log(fs,fsrc);
	GLuint program = glCreateProgram();
	graphics_assert(program);
	glCheck();
	glAttachShader(program,vs);
	glAttachShader(program,fs);
	glLinkProgram(program);
	glsl_log(program,vsrc+fsrc);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glCheck();
	return program;
}

GLint main_t::get_uniform_loc(GLuint prog,const std::string& name,GLenum type,int size) {
	glCheck();
	graphics_assert(glIsProgram(prog));
	GLint loc = glGetUniformLocation(prog,name.c_str());
	glCheck();
	if(loc == -1) graphics_error("could not get uniform " << name);
	GLchar n[100];
	GLint l;
	GLint s;
	GLenum t;
	GLint num_uniforms = 0; // uniforms and attributes may be in same array :(
	glGetProgramiv(prog,GL_ACTIVE_UNIFORMS,&num_uniforms);
	glCheck();
	for(int idx=0; idx<num_uniforms; idx++) {
		glGetActiveUniform(prog,idx,sizeof(n),&l,&s,&t,n);
		glCheck();
		if(name == n) {
			graphics_assert(!type || (type == t));
			graphics_assert(size == s);
			return loc;
		}
	}
	graphics_error(name.c_str());
	return loc; // dumb compiler
}

GLint main_t::get_attribute_loc(GLuint prog,const std::string& name,GLenum type,int size) {
	glCheck();
	graphics_assert(glIsProgram(prog));
	GLint loc = glGetAttribLocation(prog,name.c_str());
	glCheck();
	if(loc == -1) graphics_error("could not get attribute " << name);
	GLchar n[100];
	GLint l;
	GLint s;
	GLenum t;
	GLint num_attributes = 0; // uniforms and attributes may be in same array :(
	glGetProgramiv(prog,GL_ACTIVE_ATTRIBUTES,&num_attributes);
	glCheck();
	for(int idx=0; idx<num_attributes; idx++) {
		glGetActiveAttrib(prog,idx,sizeof(n),&l,&s,&t,n);
		glCheck();
		if(name == n) {
			graphics_assert(!type || (type == t));
			graphics_assert(size == s);
			return loc;
		}
	}
	graphics_error(name.c_str());
	return loc; // dumb compiler
}

void main_t::add_callback(callback_t* callback) {
	assert(std::find(_pimpl->callbacks.begin(),_pimpl->callbacks.end(),callback) == _pimpl->callbacks.end());
	_pimpl->callbacks.push_back(callback);
}

void main_t::remove_callback(callback_t* callback) {
	_pimpl_t::callbacks_t::iterator i = std::find(_pimpl->callbacks.begin(),_pimpl->callbacks.end(),callback);
	if(i != _pimpl->callbacks.end())
		_pimpl->callbacks.erase(i);
}

void main_t::read_file(const std::string& name,file_io_t* callback,intptr_t data) {
	for(_pimpl_t::file_io_impls_t::iterator i=_pimpl->file_io_impls.begin(); i!=_pimpl->file_io_impls.end(); i++)
		assert(!(((*i)->callback == callback) && ((*i)->data == data)));
	_pimpl->file_io_impls.push_back(new _file_io_impl_t(*_pimpl,name,callback,data));
}

void main_t::cancel_read_file(file_io_t* callback,intptr_t data) {
	for(_pimpl_t::file_io_impls_t::iterator i=_pimpl->file_io_impls.begin(); i!=_pimpl->file_io_impls.end(); i++) {
		if(((*i)->callback == callback) && ((*i)->data == data)) {
			(*i)->cancel();
			break;
		}
	}
}

std::string main_t::relpath(const std::string& base,const std::string& path) {
	if(!path.size()) data_error("empty path");
	if(path.at(0) == '/')
		return path;
	if(!base.size() || (base.at(base.size()-1) == '/'))
		return base+path;
	const size_t ofs = base.rfind('/');
	if(ofs == std::string::npos)
		return path;
	return base.substr(0,ofs+1) + path;
}

void main_t::load_texture(const std::string& name,texture_load_t* callback,intptr_t data) {
	if(_pimpl->textures.find(name) == _pimpl->textures.end())
		_pimpl->textures[name] = new _texture_t(*this,name);
	_pimpl->textures.find(name)->second->add(callback,data);
}

void main_t::cancel_load_texture(texture_load_t* callback,intptr_t data) {
	const _texture_t::waiting_t key = {callback,data};
	for(_pimpl_t::textures_t::iterator i=_pimpl->textures.begin(); i!=_pimpl->textures.end(); i++) {
		_texture_t::queue_t::iterator q = std::find(i->second->queue.begin(),i->second->queue.end(),key);
		if(q != i->second->queue.end()) {
			i->second->queue.erase(q);
			break;
		}
	}
}

GLuint main_t::get_shared_program(const std::string& name) {
	_pimpl_t::shared_programs_t::iterator i = _pimpl->shared_programs.find(name);
	if(i == _pimpl->shared_programs.end())
		return 0;
	return i->second;
}

GLuint main_t::set_shared_program(const std::string& name,GLuint handle) {
	_pimpl->shared_programs[name] = handle;
	return handle;
}

#ifdef __native_client__

main_t::_pimpl_t::_pimpl_t(main_t& m,void* instance_ptr): main(m), instance(static_cast<pp::Instance*>(instance_ptr)) {}

struct _platform_main_t: public pp::Instance {
public:
	explicit _platform_main_t(PP_Instance instance);
	virtual bool Init(uint32_t argc,const char* argn[],const char* argv[]) { return true; }	
	virtual bool HandleInputEvent(const pp::InputEvent& event);
	virtual void DidChangeFocus(bool focus) { has_focus = focus; }
	virtual void DidChangeView(const pp::Rect &position,const pp::Rect& clip);
	enum { MIN_WIDTH = 320, MIN_HEIGHT = 240 };
private:
	std::auto_ptr<main_t> main;
	bool has_focus;
	int width, height;
	pp::Graphics3D context;
	main_t::input_key_map_t key_map;
	main_t::input_mouse_map_t mouse_map;
private:
	void Loop();
	static void Flushed(void* data,int32_t result);
};

_platform_main_t::_platform_main_t(PP_Instance instance):
	pp::Instance(instance),
	has_focus(false), width(MIN_WIDTH), height(MIN_HEIGHT) {
	std::cout << std::endl << "** == creating instance == **" << std::endl;
	static const int32_t context_attribs[] = {
		PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 0,
		PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
		PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
		PP_GRAPHICS3DATTRIB_SAMPLES, 0,
		PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
		PP_GRAPHICS3DATTRIB_WIDTH, MIN_WIDTH,
		PP_GRAPHICS3DATTRIB_HEIGHT, MIN_HEIGHT,
		PP_GRAPHICS3DATTRIB_NONE
	};
	context = pp::Graphics3D(this,context_attribs); 
	assert(!context.is_null() && context.pp_resource());
	if(!BindGraphics(context))
		graphics_error("could not bind context");
	glSetCurrentContextPPAPI(context.pp_resource());
	main.reset(main_t::create(this,0,NULL));
	main->on_resize(width,height);
	main->init();
	std::cout << std::endl << "** == created instance == **" << std::endl;
	RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
	RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
}

void _platform_main_t::DidChangeView(const pp::Rect &position,const pp::Rect& clip) {
	try {
		std::cout << "DidChangeView((" <<
			position.x() << ',' << position.y() << ',' << position.right() << ',' << position.bottom() << "),(" <<
			clip.x() << ',' << clip.y() << ',' << clip.right() << ',' << clip.bottom() << ")) " << std::endl;
		const int new_width = std::max<int>(MIN_WIDTH,position.width()),
			new_height = std::max<int>(MIN_HEIGHT,position.height());
		if(width != new_width || height != new_height) {
			width = new_width;
			height = new_height;
			context.ResizeBuffers(width,height);
			assert(!context.is_null());
			main->on_resize(width,height);
		}
		Loop();
		std::cout << "DidChangeView() done" << std::endl;
	} catch(std::exception& e) {
		std::cerr << "Error in DidChangeView: " << e.what() << std::endl;
	}
}

static unsigned short map_nacl_key(uint32_t code) {
	struct {
		uint32_t code;
		main_t::key_t key;
	} static const key_mapping[] = {
		{27,main_t::KEY_ESC},
		{38,main_t::KEY_UP},
		{40,main_t::KEY_DOWN},
		{39,main_t::KEY_RIGHT},
		{37,main_t::KEY_LEFT},
		{33,main_t::KEY_PAGEUP},
		{34,main_t::KEY_PAGEDOWN},
		{36,main_t::KEY_HOME},
		{35,main_t::KEY_END},
		{13,main_t::KEY_RETURN},
		{8,main_t::KEY_BACKSPACE},
	};
	static const int num_key_mappings = sizeof(key_mapping)/sizeof(*key_mapping);
	for(int i=0; i<num_key_mappings; i++)
		if(code == key_mapping[i].code) {
			return key_mapping[i].key;
		}
	return code;
}

bool _platform_main_t::HandleInputEvent(const pp::InputEvent& nacl_event) {
	switch(nacl_event.GetType()) {
	case PP_INPUTEVENT_TYPE_KEYDOWN: {
		const uint32_t code = map_nacl_key(pp::KeyboardInputEvent(nacl_event).GetKeyCode());
		if(code < key_map.size())
			key_map[code] = true;
		return main->on_key_down(code,key_map,mouse_map);
	} break;
	case PP_INPUTEVENT_TYPE_KEYUP: {
		const uint32_t code = map_nacl_key(pp::KeyboardInputEvent(nacl_event).GetKeyCode());
		if(code < key_map.size())
			key_map[code] = false;
		return main->on_key_up(code,key_map,mouse_map);
	} break;
	default:
		return false;
	}
}

void _platform_main_t::Loop() {
	context.SwapBuffers(pp::CompletionCallback(Flushed,this));
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	try {
		main->_pimpl->callback();
		main->tick();
	} catch(std::exception& e) {
		std::cerr << "Error in Loop: " << e.what() << std::endl;
	}
}

void _platform_main_t::Flushed(void* data,int32_t result) {
	_platform_main_t* self = static_cast<_platform_main_t*>(data);
	self->Loop();
}

class Module: public pp::Module {
public:
	Module() : pp::Module() {}
	virtual ~Module() { glTerminatePPAPI(); }
	virtual bool Init() {
		const bool ok = glInitializePPAPI(get_browser_interface()) == GL_TRUE
			&& GetBrowserInterface(PPB_OPENGLES2_INTERFACE);
		std::cerr << "Module::Init() = " << ok << std::endl;
		return ok;
	}
	virtual pp::Instance* CreateInstance(PP_Instance instance) {
		try {
			std::cerr << "Creating instance.." << std::endl;
			return new _platform_main_t(instance);
		} catch(std::exception& e) {
			std::cerr << "Error creating instance: " << e.what() << std::endl;
			throw;
		}
	}
};

namespace pp {
	Module* CreateModule() {
		return new ::Module();
	}
}  // namespace pp

#else

main_t::_pimpl_t::_pimpl_t(main_t& m,void*): main(m) {}

struct _platform_main_t {
	_platform_main_t(main_t& m): main(m) {}
	void callback() {
		main._pimpl->callback();
	}
	bool event(const SDL_Event&);
	main_t& main;
	main_t::input_key_map_t key_map;
	main_t::input_mouse_map_t mouse_map;
};

static unsigned short map_sdl_key(const SDL_KeyboardEvent& event) {
	struct {
		SDLKey sym;
		main_t::key_t key;
	} static const key_mapping[] = {
		{SDLK_ESCAPE,main_t::KEY_ESC},
		{SDLK_UP,main_t::KEY_UP},
		{SDLK_DOWN,main_t::KEY_DOWN},
		{SDLK_RIGHT,main_t::KEY_RIGHT},
		{SDLK_LEFT,main_t::KEY_LEFT},
		{SDLK_PAGEUP,main_t::KEY_PAGEUP},
		{SDLK_PAGEDOWN,main_t::KEY_PAGEDOWN},
		{SDLK_HOME,main_t::KEY_HOME},
		{SDLK_END,main_t::KEY_END},
		{SDLK_RETURN,main_t::KEY_RETURN},
		{SDLK_BACKSPACE,main_t::KEY_BACKSPACE},
	};
	static const int num_key_mappings = sizeof(key_mapping)/sizeof(*key_mapping);
	for(int i=0; i<num_key_mappings; i++)
		if(event.keysym.sym == key_mapping[i].sym) {
			return key_mapping[i].key;
		}
	short code = event.keysym.unicode;
	if(!code) {
		code = event.keysym.sym;
		std::cout<<"(could not map key "<<code<<')'<<std::endl;
	}
	return code;
}

namespace { struct _discard_event {}; } // throw this to suppress handling and cerr of an event

static main_t::mouse_button_t map_sdl_mouse(const SDL_MouseButtonEvent& sdl) {
	switch(sdl.button) {
	case SDL_BUTTON_LEFT: return main_t::MOUSE_LEFT;
	case SDL_BUTTON_MIDDLE: return main_t::MOUSE_MIDDLE;
	case SDL_BUTTON_RIGHT: return main_t::MOUSE_RIGHT;
	case SDL_BUTTON_WHEELUP: throw _discard_event();
	case SDL_BUTTON_WHEELDOWN: throw _discard_event();
	default:
		std::cout << "SDL button "<<sdl.button<<" not mapped"<<std::endl;
		throw _discard_event();
	};
}

bool _platform_main_t::event(const SDL_Event& sdl) {
	switch(sdl.type) {
	case SDL_KEYDOWN: {
		const unsigned short code = map_sdl_key(sdl.key);
		if(code < key_map.size())
			key_map[code] = true;
		return main.on_key_down(code,key_map,mouse_map);
	} break;
	case SDL_KEYUP: {
		const unsigned short code = map_sdl_key(sdl.key);
		if(code < key_map.size())
			key_map[code] = false;
		return main.on_key_up(code,key_map,mouse_map);
	} break;
	case SDL_MOUSEBUTTONDOWN: {
		main_t::mouse_button_t button = map_sdl_mouse(sdl.button);
		if(button < mouse_map.size())
			mouse_map[button] = true;
		return main.on_mouse_down(sdl.button.x,sdl.button.y,button,key_map,mouse_map);
	} break;
	case SDL_MOUSEBUTTONUP: {
		main_t::mouse_button_t button = map_sdl_mouse(sdl.button);
		if(button < mouse_map.size())
			mouse_map[button] = false;
		return main.on_mouse_up(sdl.button.x,sdl.button.y,button,key_map,mouse_map);
	} break;
	case SDL_MOUSEMOTION: {
		if(!sdl.motion.state)
			throw _discard_event();
		return main.on_mouse_down(sdl.motion.x,sdl.motion.y,main_t::MOUSE_DRAG,key_map,mouse_map);
	} break;
	default:
		return false;
	}
}

int main(int argc,char** args) {
	fprintf(stderr,"%s (c) William Edwards, 2012.  All rights reserved.\n",main_t::game_name);
	fprintf(stderr,"(built %s, %s)\n",build_timestamp,git_info);
	if(SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr,"Unable to initialise SDL: %s\n",SDL_GetError());
		return EXIT_FAILURE;
	}
	atexit(SDL_Quit);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	SDL_Surface* window = SDL_SetVideoMode(800,600,24,SDL_OPENGL);
	if(!window) {
		fprintf(stderr,"Unable to create SDL window: %s\n",SDL_GetError());
		return EXIT_FAILURE;
	}
	const GLenum glew_err = glewInit();
	if(GLEW_OK != glew_err) {
		fprintf(stderr,"Cannot initialise GLEW: %s\n",glewGetErrorString(glew_err));
		return EXIT_FAILURE;
	}
	SDL_WM_SetCaption(main_t::game_name,main_t::game_name);
	std::auto_ptr<main_t> main(main_t::create(NULL,argc,args));
	main->init();
	_platform_main_t platform(*main.get());
	main->on_resize(window->w,window->h);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	while(main->tick()) {
		// flush graphics
		SDL_GL_SwapBuffers();
		SDL_Flip(window);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		// handle callbacks
		platform.callback();
		// handle events
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT)
				return EXIT_SUCCESS;
			try {
				if(!platform.event(event)) {
					static int last_ignored_event = -1;
					if(event.type != last_ignored_event) {
						last_ignored_event = event.type;
						std::cerr << "event(" << (int)event.type << ") not handled" << std::endl;
					}
				}
			} catch(_discard_event& de) {}
		}
	}
	return EXIT_SUCCESS;
}

#endif