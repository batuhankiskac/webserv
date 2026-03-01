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

	void _parseIndex(const std::vector<std::string>& tokens, size_t& i);
	void _parseAllowMethods(const std::vector<std::string>& tokens, size_t& i);
	void _parsePath(const std::vector<std::string>& tokens, size_t& i);
	void _parseRoot(const std::vector<std::string>& tokens, size_t& i);
	void _parseUploadStore(const std::vector<std::string>& tokens, size_t& i);
	void _parseUploadEnable(const std::vector<std::string>& tokens, size_t& i);
	void _parseCgiPath(const std::vector<std::string>& tokens, size_t& i);
	void _parseCgiExt(const std::vector<std::string>& tokens, size_t& i);
	void _parseReturn(const std::vector<std::string>& tokens, size_t& i);
	void _parseAutoIndex(const std::vector<std::string>& tokens, size_t& i);

public:
	LocationBlock(const std::string& path);
	~LocationBlock();

	void parseLocationBlock(const std::vector<std::string>& tokens, size_t& i);

	const std::vector<std::string>& getIndex() const { return _index; }
	const std::vector<std::string>& getAllowMethods() const { return _allowMethods; }
	const std::string& getPath() const { return _path; }
	const std::string& getRoot() const { return _root; }
	const std::string& getUploadStore() const { return _uploadStore; }
	const std::string& getCgiPath() const { return _cgiPath; }
	const std::string& getCgiExt() const { return _cgiExt; }
	const std::string& getReturnUrl() const { return _returnUrl; }
	bool getUploadEnable() const { return _uploadEnable; }
	bool getAutoIndex() const { return _autoIndex; }
	int getReturnCode() const { return _returnCode; }

	void setIndex(const std::vector<std::string>& index) { _index = index; }
	void setAllowMethods(const std::vector<std::string>& allowMethods) { _allowMethods = allowMethods; }
	void setPath(const std::string& path) { _path = path; }
	void setRoot(const std::string& root) { _root = root; }
	void setUploadStore(const std::string& uploadStore) { _uploadStore = uploadStore; }
	void setCgiPath(const std::string& cgiPath) { _cgiPath = cgiPath; }
	void setCgiExt(const std::string& cgiExt) { _cgiExt = cgiExt; }
	void setReturnUrl(const std::string& returnUrl) { _returnUrl = returnUrl; }
	void setUploadEnable(bool uploadEnable) { _uploadEnable = uploadEnable; }
	void setAutoIndex(bool autoIndex) { _autoIndex = autoIndex; }
	void setReturnCode(int returnCode) { _returnCode = returnCode; }
};

#endif
