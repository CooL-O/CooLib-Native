#pragma once

class cLibInfo
{
public:
	HMODULE hlib;
	std::string path;
	std::string name;
	nlohmann::json j;
	bool loaded;
	cLibInfo(HMODULE _hlib, std::string _path, nlohmann::json _j) {
		hlib = _hlib;
		path = _path;
		j = _j;
		name = j["AppID"].get<std::string>();
		loaded = false;
	}
};

class cTopological
{
public:
	std::string name;
	std::vector<std::string > libNames;
};

extern std::vector<cLibInfo> libList; // �����

extern void loadLib();
extern void unloadLib();
extern void reloadLib();

bool versionMatch(std::string rver, std::string ver);
std::vector<cLibInfo> LibSort(const std::vector<cLibInfo> sortLib);
std::vector<std::string> TSort(const std::vector<cTopological> sortT);
std::vector<cLibInfo>::iterator libFind(std::vector<cLibInfo>::iterator _First, const std::vector<cLibInfo>::iterator _Last, const std::string& _Val);
