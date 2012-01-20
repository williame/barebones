#include <iostream>

#include "barebones/main.hpp"

const char* const main_t::game_name = "TEMPLATE"; // window titles etc

class main_game_t: public main_t {
public:
	main_game_t(void* platform_ptr): main_t(platform_ptr) {}
	void init();
	bool tick();
	// debug just print state
	bool on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse);
	bool on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse);
};

void main_game_t::init() {
	// e.g. create shaders, bind uniforms, make static vbos etc
}

bool main_game_t::tick() {
	// move game-world forward, draw it
	return true; // return false to exit program
}

main_t* main_t::create(void* platform_ptr,int argc,char** args) {
	return new main_game_t(platform_ptr);
}

// pure debug code to be chopped out below:

static void print_debug_input_map(const main_t::input_key_map_t& map,const main_t::input_mouse_map_t& mouse) {
	for(size_t i=0; i<map.size(); i++)
		if(map[i])
			std::cout << "key " << i << " down" << std::endl;
	for(size_t i=0; i<mouse.size(); i++)
		if(mouse[i])
			std::cout << "mouse " << i << " down" << std::endl;
}

bool main_game_t::on_key_down(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	std::cout << "KEY " << code << " DOWN" << std::endl;
	print_debug_input_map(map,mouse);
	return false;
}

bool main_game_t::on_key_up(short code,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	std::cout << "KEY " << code << " UP" << std::endl;
	print_debug_input_map(map,mouse);
	return false;
}

bool main_game_t::on_mouse_down(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	std::cout << "MOUSE " << x << ',' << y << ',' << button << " DOWN" << std::endl;
	print_debug_input_map(map,mouse);
	return false;
}

bool main_game_t::on_mouse_up(int x,int y,mouse_button_t button,const input_key_map_t& map,const input_mouse_map_t& mouse) {
	std::cout << "MOUSE " << x << ',' << y << ',' << button << " UP" << std::endl;
	print_debug_input_map(map,mouse);
	return false;
}
