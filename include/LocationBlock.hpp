#ifndef LOCATION_BLOCK_HPP
#define LOCATION_BLOCK_HPP

#include <map>
#include <string>
#include <vector>

class LocationBlock {
private:
	std::vector<std::string> _index;
	std::vector<std::string> _allowMethods;
	std::string _path;
	std::string _root;
	std::string _uploadStore;
	std::string _cgiPath;
	std::string _cgiExt;
	std::string _returnUrl;
	bool _uploadEnable;
	bool _autoIndex;
	int _returnCode;

	typedef void (LocationBlock::*func)(const std::vector<std::string>&, size_t&);

	std::map<std::string, func> _directives;
	void _setupDirectives();

	void _parseSingleValue(const std::vector<std::string>& tokens, size_t& i, std::string& dest, const std::string& name);
	void _parseOnOff(const std::vector<std::string>& tokens, size_t& i, bool& dest, const std::string& name);

	void _parseIndex(const std::vector<std::string>& tokens, size_t& i);
	void _parseAllowMethods(const std::vector<std::string>& tokens, size_t& i);
	void _parseReturn(const std::vector<std::string>& tokens, size_t& i);

	void _parsePath(const std::vector<std::string>& tokens, size_t& i) { _parseSingleValue(tokens, i, _path, "path"); }
	void _parseRoot(const std::vector<std::string>& tokens, size_t& i) { _parseSingleValue(tokens, i, _root, "root"); }
	void _parseUploadStore(const std::vector<std::string>& tokens, size_t& i) { _parseSingleValue(tokens, i, _uploadStore, "upload_store"); }
	void _parseCgiPath(const std::vector<std::string>& tokens, size_t& i) { _parseSingleValue(tokens, i, _cgiPath, "cgi_path"); }
	void _parseCgiExt(const std::vector<std::string>& tokens, size_t& i) { _parseSingleValue(tokens, i, _cgiExt, "cgi_ext"); }
	void _parseUploadEnable(const std::vector<std::string>& tokens, size_t& i) { _parseOnOff(tokens, i, _uploadEnable, "upload_enable"); }
	void _parseAutoIndex(const std::vector<std::string>& tokens, size_t& i) { _parseOnOff(tokens, i, _autoIndex, "autoindex"); }

public:
	LocationBlock(const std::string& path);
	~LocationBlock();

	void parseLocationBlock(const std::vector<std::string>& tokens, size_t& i);
};

#endif
