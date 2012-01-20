#include <cstdarg>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <iostream>

#include "xml.hpp"
#include "main.hpp"

struct xml_parser_t::token_t {
	token_t(xml_type_t t,const char* s): 
		type(t), start(s), len(0),
		visit(false), error(NULL),
		parent(NULL), first_child(NULL), next_peer(NULL) {}
	~token_t() { free(error); delete first_child; delete next_peer; }
	token_t* add_child(xml_type_t t,const char* s) {
		if(first_child)
			return first_child->add_peer(t,s);
		first_child = new token_t(t,s);
		first_child->parent = this;
		return first_child;
	}
	token_t* add_peer(xml_type_t t,const char* s) {
		if(!next_peer) {
			next_peer = new token_t(t,s);
			next_peer->parent = parent;
			return next_peer;
		} else {
			token_t* peer = next_peer;
			while(peer->next_peer)
				peer = peer->next_peer;
			peer = peer->next_peer = new token_t(t,s);
			peer->parent = parent;
			return peer;
		}
	}
	const xml_type_t type; 
	const char* const start;
	size_t len;
	mutable bool visit;
	mutable char* error;
	bool set_error(const char* fmt,...) const;
	token_t *parent, *first_child, *next_peer;
	std::string str() const {
		return std::string(start,len);
	}
	std::string repr() const {
		std::string ret;
		for(size_t i=0; i<len; i++)
			ret += (char)start[i];
		return ret;
	}
	std::string path() const {
		std::string ret = str();
		for(const token_t* p=parent; p; p = p->parent)
			ret = p->str() + '/' + ret;
		return ret;
	}
	bool equals(const char* s) const {
		if(strlen(s) != len) return false;
		for(size_t i=0; i<len; i++)
			if(s[i] != start[i])
				return false;
		return true;
	}
	bool equals(const token_t* t) const {
		if(t->len != len) return false;
		for(size_t i=0; i<len; i++)
			if(start[i] != t->start[i])
				return false;
		return true;
	}
};

bool xml_parser_t::token_t::set_error(const char* fmt,...) const {
	va_list args;
	va_start(args,fmt);
	// had problems finding vsnprintf
	const size_t len = __builtin_vsnprintf(NULL,0,fmt,args);
	if(char* e = (char*)malloc(len+1)) {
		free(error);
		error = e;
		va_start(args,fmt);
		__builtin_vsnprintf(error,len+1,fmt,args);
		return true;
	}
	return false;
}

static const char* eat_whitespace(const char* ch) { while(*ch && *ch <= ' ') ch++; return ch; }
static const char* eat_name(const char* ch) { while((*ch>' ')&&!strchr("/>=",*ch)) ch++; return ch; }

static bool starts_with(const char* str,const char* pre) {
	while(*pre)
		if(*pre++!=*str++) return false;
	return true;
}

xml_parser_t::xml_parser_t(): title("<empty xml>"), doc(NULL) {}

xml_parser_t::xml_parser_t(const xml_parser_t& copy): title(copy.title), buf(copy.buf), doc(NULL) {
	parse();
}

xml_parser_t::xml_parser_t(const std::string t,const char* xml):
	title(t), buf(xml), doc(NULL) {
	parse();
}

xml_parser_t& xml_parser_t::operator=(const xml_parser_t& copy) {
	delete doc; doc = NULL;
	const_cast<std::string&>(title) = copy.title;
	const_cast<std::string&>(buf) = copy.buf;
	parse();
	return *this;
}

xml_parser_t::xml_parser_t(const std::string t,const std::string xml):
	title(t), buf(xml), doc(NULL) {
	parse();
}
	
void xml_parser_t::parse() {
	if(doc) return;
	if(!buf.size())
		data_error("empty document"); // outside try so no dom objects created
	const char *ch = buf.c_str();
	token_t* tok = NULL;
	try {
		ch = eat_whitespace(ch);
		if(*ch!='<')
			data_error("malformed XML, expecting <");
		bool in_tag = false;
		const char *prev = 0;
		while(*ch) {
			if(prev == ch)
				data_error("internal error parsing "<<ch);
			prev = ch;
			if('<' == *ch) {
				if(in_tag)
					data_error("unexpected <");
				if(tok && (XML_DATA == tok->type)) {
					if(eat_whitespace(tok->start) == ch)
						data_error("unexpected empty token "<<tok->repr());
					tok->len = (ch-tok->start);
					tok = tok->parent;
				}
				if(starts_with(ch,"<!--")) {
					if(const char* t = strstr(ch,"-->"))
						ch = eat_whitespace(t+3);
					else data_error("unclosed comment");
				} else if(starts_with(ch,"<?")) {
					if(const char* t = strstr(ch,"?>"))
						ch = eat_whitespace(t+2);
					else data_error("unclosed processing tag");
				} else {
					ch = eat_whitespace(ch+1);
					if('/' == *ch) {
						if(!tok) data_error("unexpected close of tag");
						ch = eat_whitespace(ch+1);
						token_t* open = tok;
						if(tok->type != XML_OPEN) {
							if(tok->type != XML_DATA)
								data_error("expecting closing tag to be after data");
							open = tok->parent;
						}
						tok = open->add_peer(XML_CLOSE,ch);
						ch = eat_name(ch);
						tok->len = ch - tok->start;
						if(!tok->equals(open))
							data_error(tok->str()<<" mismatches "<<open->str());
						ch = eat_whitespace(ch);
						if('>'!=*ch) data_error("unclosed close tag "<<*ch);
						const char* peek = eat_whitespace(++ch);
						if(*peek == '<')
							ch = peek;
						else if(!tok->parent) {
							if(*peek)
								data_error("unexpected content at top level: "<<peek);
							break; // all done
						}
						if(XML_OPEN!=tok->parent->type)
							data_error("unexpected "<<tok->repr()<<" after "<<tok->parent->repr());
						tok = tok->parent;
					} else {
						in_tag = true;
						if(!tok)
							doc = tok = new token_t(XML_OPEN,ch);
						else if(XML_DATA == tok->type)
							tok = tok->add_peer(XML_OPEN,ch);
						else if(XML_OPEN == tok->type)
							tok = tok->add_child(XML_OPEN,ch);
						else data_error("was not expecting a new tag after "<<tok->repr());
						ch = eat_name(ch);
						tok->len = ch - tok->start;
						ch = eat_whitespace(ch);
					}
				}
			} else if(!tok) {
				data_error("expecting <");
			} else if(XML_DATA == tok->type) {
				if('>' == *ch)
					data_error("stray > found outside tag");
				ch++;
			} else if('>' == *ch) {
				if(XML_OPEN == tok->type) {
					const char* peek = ch+1;
					if(!*peek) break;
					peek = eat_whitespace(peek);
					if(*peek != '<')
						tok = tok->add_child(XML_DATA,ch);
					else
						ch = peek;
				} else if(XML_KEY != tok->type)
					tok = tok->parent;
				in_tag = false;
			} else if('=' == *ch) {
				if(XML_KEY != tok->type)
					data_error("was not expecting = after "<<tok->repr());
				if(tok->first_child)
					data_error("was not expecting = after "<<tok->first_child->repr());
				ch = eat_whitespace(ch+1);
				if('\"' != *ch)
					data_error("was expecting \" after "<<tok->repr());
				ch++;
				tok = tok->add_child(XML_VALUE,ch);
				ch = strchr(ch,'\"');
				if(!ch) data_error("unclosed attribute "<<tok->parent->repr());
				tok->len = (ch - tok->start);
				tok = tok->parent->parent;
				ch = eat_whitespace(ch+1);
			} else if('/' == *ch) {
				if(XML_OPEN != tok->type)
					data_error("not expecting / after "<<tok->repr());
				token_t* close = tok->add_peer(XML_CLOSE,tok->start);
				close->len = tok->len;
				tok = tok->parent;
				ch = eat_whitespace(ch+1);
				if('>' != *ch)
					data_error("not expecting "<<*ch<<" after "<<close->repr());
				in_tag = false;
				const char* peek = eat_whitespace(++ch);
				if(*peek == '<')
					ch = peek;
				else
					tok = tok->add_child(XML_DATA,ch++);
			} else if(XML_OPEN == tok->type) {
				tok = tok->add_child(XML_KEY,ch);
				ch = eat_name(ch);
				tok->len = (ch - tok->start);
				ch = eat_whitespace(ch);
			} else 
				data_error("did not understand "<<*ch<<" after"<<tok->repr());
		}
		/*## BUG/LIMITATION/OMISSION ##*
		#### when the input stream is consumed, we aren't checking here that the root tag
		#### and other open tags are in fact closed
		#####*/
	} catch(data_error_t& de) {
		if(!ch) ch = buf.c_str() + buf.size();
		std::cerr << "Error parsing " << title << " @" << (ch-buf.c_str()) << ": " << de.what() << std::endl;
		if(!doc)
			tok = doc = new token_t(XML_ERROR,ch);
		else
			tok = tok->add_peer(XML_ERROR,ch);
		tok->len = buf.size()-(ch-buf.c_str());
		tok->error = strdup(de.what());
		throw;
	}
}
	
xml_parser_t::~xml_parser_t() {
	delete doc;
}

xml_type_t xml_walker_t::type() const {
	if(!ok()) data_error("no token");
	if(tok->error) return XML_ERROR;
	return tok->type;
}

bool xml_walker_t::next() {
	if(!ok()) data_error("no token");
	if(tok->first_child) {
		tok = tok->first_child;
		return true;
	}
	if(tok->next_peer) {
		tok = tok->next_peer;
		return true;
	}
	while(true) {
		tok = tok->parent;
		if(!tok) return false;
		if(tok->next_peer) {
			tok = tok->next_peer;
			return true;
		}
	}
}

void xml_walker_t::get_tag() {
	if(!ok()) data_error("no token");
	if(XML_KEY == tok->type)
		tok = tok->parent;
	if(XML_OPEN != tok->type)
		data_error("was expecting an open tag, got "<<tok->repr());
}

std::string xml_walker_t::tag() const {
	if(!ok()) data_error("no token");
	const xml_parser_t::token_t* tag = tok;
	if(XML_KEY == tag->type)
		tag = tag->parent;
	if(XML_OPEN != tag->type)
		data_error("was expecting an open tag, got "<<tok->repr());
	return tag->str();
}

void xml_walker_t::get_key(const char* key) {
	get_tag();
	for(xml_parser_t::token_t* child = tok->first_child; child; child = child->next_peer)
		if((XML_KEY == child->type) && child->equals(key)) {
			tok = child;
			tok->visit = true;
			return;
		}
	data_error(key << " not found in " << tok->str() << " tag");
}

bool xml_walker_t::has_key(const char* key) {
	get_tag();
	for(xml_parser_t::token_t* child = tok->first_child; child; child = child->next_peer)
		if((XML_KEY == child->type) && child->equals(key)) {
			child->visit = true;
			return true;
		}
	return false;
}

xml_walker_t& xml_walker_t::get_child(const char* tag) {
	if(get_child(tag,0)) return *this;
	data_error(tok->str()<<" tag has no child tag called "<<tag);
}

xml_walker_t& xml_walker_t::get_peer(const char* tag) {
	return up().get_child(tag);
}

bool xml_walker_t::get_child(const char* tag,size_t i) {
	get_tag();
	for(xml_parser_t::token_t* child = tok->first_child; child; child = child->next_peer)
		if((XML_OPEN == child->type) && child->equals(tag) && (!i--)) {
			tok = child;
			tok->visit = true;
			return true;
		}
	return false;
}

bool xml_walker_t::has_child(const char* tag) {
	get_tag();
	for(xml_parser_t::token_t* child = tok->first_child; child; child = child->next_peer)
		if((XML_OPEN == child->type) && child->equals(tag))
			return true;
	return false;
}
     
bool xml_walker_t::first_child() {
	get_tag();
	for(xml_parser_t::token_t* child = tok->first_child; child; child = child->next_peer)
		if(XML_OPEN == child->type) {
			tok = child;
			tok->visit = true;
			return true;
		}
	return false;
}

bool xml_walker_t::next_peer() {
	get_tag();
	const xml_parser_t::token_t* peer = tok->next_peer;
	while(peer && (XML_OPEN != peer->type))
		peer = peer->next_peer;
	if(peer) {
		tok = peer;
		tok->visit = true;
		return true;
	}
	return false;
}

xml_walker_t& xml_walker_t::up() {
	get_tag();
	if(!tok->parent)
		data_error("cannot go up from root");
	tok = tok->parent;
	return *this;
}

xml_walker_t& xml_walker_t::check(const char* tag) {
	get_tag();
	if(strncmp(tag,tok->start,strlen(tag)))
		data_error("expecting "<<tag<<" tag, got "<<tok->str());
	tok->visit = true;
	return *this;
}

std::string xml_walker_t::value_string(const char* key) {
	get_key(key);
	if(!tok->first_child || (XML_VALUE != tok->first_child->type))
		data_error("expecting key "<<tok->path()<<" to have a value child");
	tok = tok->first_child;
	tok->visit = true;
	std::string str = tok->str();
	tok = tok->parent;
	return str;
}

float xml_walker_t::value_float(const char* key) {
	const std::string value(value_string(key));
	if(!value.size()) data_error(tok->path()<<" should be a float");
	tok = tok->first_child; // ensure errors are assigned to child leaf
	errno = 0;
	char* endptr;
	const float val = strtof(value.c_str(),&endptr);
	if(errno) data_error("could not convert "<<tok->path()<<" to float: "<<value<<" ("<<errno<<": "<<strerror(errno));
	if(endptr != value.c_str()+value.size()) data_error(tok->path()<<" is not a float: "<<value);
	if(!std::isnormal(val) && FP_ZERO!=std::fpclassify(val)) data_error(tok->path()<<" is not a valid float: "<<value);
	tok = tok->parent;
	return val;
}

int xml_walker_t::value_int(int def,const char* key) {
	if(has_key(key)) return value_int(key);
	return def;
}

int xml_walker_t::value_int(const char* key) {
	const std::string value(value_string(key));
	if(!value.size()) data_error(tok->path()<<" should be an int");
	tok = tok->first_child; // ensure errors are assigned to child leaf
	errno = 0;
	char* endptr;
	const int i = strtol(value.c_str(),&endptr,10);
	if(errno) data_error("could not convert "<<tok->path()<<" to int: "<<value<<" ("<<errno<<": "<<strerror(errno));
	if(endptr != value.c_str()+value.size()) data_error(tok->path()<<" is not an int: "<<value);
	tok = tok->parent;
	return i;
}

bool xml_walker_t::value_bool(bool def,const char* key) {
	if(has_key(key)) return value_bool(key);
	return def;
}

bool xml_walker_t::value_bool(const char* key) {
	const std::string value(value_string(key));
	if(!value.size()) data_error(tok->path()<<" should be boolean");
	if(value == "true") return true;
	if(value == "false") return false;
	tok = tok->first_child; // errors are assigned to child leaf
	data_error(tok->path()<<" is not boolean: "<<value);
}

uint64_t xml_walker_t::value_hex(const char* key) {
	const std::string value(value_string(key));
	if(!value.size() || value.size() > 16) data_error(tok->path()<<" should be uint64_t");
	tok = tok->first_child; // ensure errors are assigned to child leaf
	uint64_t ret = 0;
	for(const char* ch = value.c_str(); *ch; ch++) {
		ret <<= 4;
		if(*ch >= '0' && *ch <= '9')
			ret |= *ch - '0';
		else if(*ch >= 'a' && *ch <= 'f')
			ret |= 10 + *ch - 'a';
		else if(*ch >= 'A' && *ch <= 'F')
			ret |= 10 + *ch - 'A';
		else
			data_error(tok->path()<<" should be uint64_t");
	}
	tok = tok->parent;
	return ret;
}

std::string xml_walker_t::get_data_as_string() {
	get_tag();
	xml_parser_t::token_t* child = tok->first_child;
	while(child && (XML_DATA != child->type))
		child = child->next_peer;
	if(!child)
		data_error("expecting tag "<<tok->path()<<" to have data");
	if(child->next_peer)
		data_error("cannot cope that tag "<<tok->path()<<" has nested tags when extracting data");
	child->visit = true;
	std::string str = child->str();
	return str;
}
 
size_t xml_walker_t::ofs() const {
	if(!ok()) data_error("no token");
	return tok->start - parser.buf.c_str();
}

size_t xml_walker_t::len() const {
	if(!ok()) data_error("no token");
	return tok->len;
}

std::string xml_walker_t::str() const {
	if(!ok()) data_error("no token");
	return tok->str();
}

const char* xml_walker_t::error_str() const {
	if(!ok()) data_error("no token");
	return tok->error;
}

bool xml_walker_t::visited() const {
	if(!ok()) data_error("no token");
	return tok->visit;
}

xml_walker_t::xml_walker_t(xml_parser_t& p,const xml_parser_t::token_t* t): parser(p), tok(t) {}

xml_walker_t xml_parser_t::walker() {
	if(!doc) parse();
	return xml_walker_t(*this,doc);
}
