#ifndef LOCATION_BLOCK_HPP
#define LOCATION_BLOCK_HPP

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

	void _parseSingleValue(const std::vector<std::string>& tokens, size_t& i, std::string& dest, const std::string& name);
	void _parseOnOff(const std::vector<std::string>& tokens, size_t& i, bool& dest, const std::string& name);

	void _parseIndex(const std::vector<std::string>& tokens, size_t& i);
	void _parseAllowMethods(const std::vector<std::string>& tokens, size_t& i);
	void _parseReturn(const std::vector<std::string>& tokens, size_t& i);

public:
	LocationBlock(const std::string& path);

	void parseLocationBlock(const std::vector<std::string>& tokens, size_t& i);

	const std::string& getPath() const;
	const std::string& getRoot() const;
	const std::vector<std::string>& getIndex() const;
	const std::vector<std::string>& getAllowMethods() const;
	const std::string& getUploadStore() const;
	const std::string& getCgiPath() const;
	const std::string& getCgiExt() const;
	const std::string& getReturnUrl() const;
	int getReturnCode() const;
	bool getUploadEnable() const;
	bool getAutoIndex() const;
};

#endif
