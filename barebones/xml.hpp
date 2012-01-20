#ifndef __XML_HPP__
#define __XML_HPP__

#include <string>
#include <sstream>
#include <inttypes.h>

class xml_walker_t;

class xml_parser_t {
public:
	struct token_t;
	xml_parser_t();
	xml_parser_t(const xml_parser_t& copy);
	xml_parser_t(const std::string title,const char* xml);
	xml_parser_t(const std::string title,const std::string xml);
	~xml_parser_t();
	xml_parser_t& operator=(const xml_parser_t& copy);
	xml_walker_t walker();
	const std::string title;
	const std::string buf;
private:
	void parse();
	token_t *doc;
};

enum xml_type_t {
	XML_IGNORE,
	XML_OPEN,
	XML_CLOSE,
	XML_KEY,
	XML_VALUE,
	XML_DATA,
	XML_ERROR,
	XML_NUM_TYPES
};

class xml_walker_t {
public:
	// depth-first traversal; don't mix with navigation API unless you know the inner workings
	bool next();
	bool ok() const { return tok; }
	// navigation API; preferred way of extracting things
	xml_walker_t& check(const char* tag);
	xml_walker_t& get_child(const char* tag);
	xml_walker_t& get_peer(const char* tag);
	bool has_child(const char* tag);
	bool get_child(const char* tag,size_t i);
	bool first_child();
	bool next_peer();
	xml_walker_t& up();
	// extract attributes
	bool has_key(const char* key = "value");
	float value_float(const char* key = "value");
	std::string value_string(const char* key = "value");
	int value_int(int def,const char* key = "value");
	int value_int(const char* key = "value");
	bool value_bool(bool def,const char* key = "value");
	bool value_bool(const char* key = "value");
	uint64_t value_hex(const char* key = "value");
	std::string get_data_as_string();
	// query current node
	xml_type_t type() const;
	size_t ofs() const;
	size_t len() const;
	std::string tag() const;
	std::string str() const;
	const char* error_str() const;
	bool visited() const;
	friend class xml_parser_t;
private:
	xml_walker_t(xml_parser_t& parser,const xml_parser_t::token_t* tok);
	xml_parser_t& parser;
	const xml_parser_t::token_t* tok;
	void get_key(const char* key);
	void get_tag();
};

#endif //__XML_HPP__

