#ifndef __MAIN_HPP__
#define __MAIN_HPP__

#include <vector>
#include <algorithm>
#include <bitset>
#include <string>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <cmath> //#defines log2 etc, so must get its header-guard in before GLM #undefs it (_fixes.hpp) on NaCL

#ifdef __native_client__
	#include <GLES2/gl2.h>
#elif defined(MINGW_X)
	#define GLEW_STATIC
	#include "../external/glew.h"
	#include <GL/gl.h>
#else
	#define GLEW_STATIC
	#include <GL/glew.h>
	#include <GL/gl.h>
#endif

struct _platform_main_t;

class main_t {
	friend struct _platform_main_t;
public:
	virtual ~main_t();
	// init and re-init
	virtual void init() = 0;
	virtual void on_resize(int width,int height) {
		this->width = width; this->height = height;
		glViewport(0,0,width,height);
	}
	// graphics utils
	GLuint create_program(const char* vertex,const char* fragment);
	GLint get_uniform_loc(GLuint prog,const std::string& name,GLenum type=0,int size=1); 
	GLint get_attribute_loc(GLuint prog,const std::string& name,GLenum type=0,int size=1);
	// file io
	struct file_io_t {
		virtual void on_io(const std::string& name,bool ok,const std::string& bytes,intptr_t data) = 0;
	};
	void read_file(const std::string& name,file_io_t* callback,intptr_t data);
	void cancel_read_file(file_io_t* callback,intptr_t data);
	static std::string relpath(const std::string& base,const std::string& path);
	// shared textures
	struct texture_load_t {
		virtual void on_texture_loaded(const std::string& name,GLuint handle,intptr_t data) = 0;
	};
	void load_texture(const std::string& name,texture_load_t* callback,intptr_t data);
	void cancel_load_texture(texture_load_t* callback,intptr_t data);
	// shared shader programs
	GLuint get_shared_program(const std::string& name);
	GLuint set_shared_program(const std::string& name,GLuint handle);
	// main loop
	virtual bool tick() = 0; // called after event handlers
	// async callbacks on next loop, called before event handlers and before tick()
	struct callback_t {
		virtual void on_fire() = 0;
	};
	void add_callback(callback_t* callback);
	void remove_callback(callback_t* callback);
	// input handling
	enum key_t {
		// pretty mnemonics
		KEY_BACKSPACE = '\b',
		KEY_RETURN = '\n',
		KEY_ESC = 27,
		KEY_LAST_ASCII = 127,
		// start of special keys
		KEY_LEFT,
		KEY_RIGHT,
		KEY_UP,
		KEY_DOWN,
		KEY_PAGEUP,
		KEY_PAGEDOWN,
		KEY_HOME,
		KEY_END,
		KEY_CAPS,
		KEY_NUM,
		// modifiers are in here too; ALT and MODE not supported on all platforms
		KEY_SHIFT,
		KEY_CTRL,
		KEY_LAST
	};
	typedef std::bitset<KEY_LAST> input_key_map_t;
	enum mouse_button_t {
		MOUSE_LEFT,
		MOUSE_MIDDLE,
		MOUSE_RIGHT,
		MOUSE_LAST,
		MOUSE_DRAG = MOUSE_LAST
	};
	typedef std::bitset<MOUSE_LAST> input_mouse_map_t;
	virtual bool on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) { return false; }
	virtual bool on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) { return false; }
	virtual bool on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) { return false; }
	virtual bool on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) { return false; }
	// factory
	static main_t* create(void* platform_ptr,int argc,char** args);
	static const char* const game_name;
	struct _pimpl_t;
protected:
	main_t(void* platform_ptr);
	int width, height;
private:
	_pimpl_t* _pimpl;
};

class gl_exception_t: public std::exception {
public:
	gl_exception_t(const std::string& m): msg(m) {}
	~gl_exception_t() throw() {}
	const char* what() const throw() { return msg.c_str(); }
protected:
	const std::string msg;	
};

#define graphics_error(expr) { \
	std::stringstream buf(std::ios_base::out|std::ios_base::ate); \
	buf << __FILE__ << ':' << __LINE__ << " graphics error (" << glGetError() << "): " << expr; \
	throw gl_exception_t(buf.str()); \
}
#define graphics_assert(test) if(!(test)) graphics_error(#test);

#define glCheck(...) { \
	if(GL_NO_ERROR != glGetError()) \
		graphics_error(#__VA_ARGS__); \
}

class data_error_t: public std::exception {
public:
	data_error_t(const std::string& m): msg(m) {}
	~data_error_t() throw() {}
	const char* what() const throw() { return msg.c_str(); }
protected:
	const std::string msg;	
};

#define data_error(expr) { \
	std::stringstream buf(std::ios_base::out|std::ios_base::ate); \
	buf << __FILE__ << ':' << __LINE__ << " data error: " << expr; \
	throw data_error_t(buf.str()); \
}

class panic_t: public std::exception {
public:
	panic_t(const std::string& m): msg(m) {}
	~panic_t() throw() {}
	const char* what() const throw() { return msg.c_str(); }
protected:
	const std::string msg;	
};

#define panic(expr) { \
	std::stringstream buf(std::ios_base::out|std::ios_base::ate); \
	buf << __FILE__ << ':' << __LINE__ << " PANIC: " << expr; \
	throw panic_t(buf.str()); \
}


#endif//__MAIN_HPP__
