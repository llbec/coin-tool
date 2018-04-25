#include "utils.h"
#include "protocol.h"
#include "net.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>

using namespace std;

boost::filesystem::path GetCoinToolDir()
{
    namespace fs = boost::filesystem;
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\UlordCore
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\UlordCore
    // Mac: ~/Library/Application Support/UlordCore
    // Unix: ~/.ulordcore
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "CoinTool";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    return pathRet / "Library/Application Support/CoinTool";
#else
    // Unix
    return pathRet / ".cointool";
#endif
#endif
}


boost::filesystem::path GetFile(const std::string & filename)
{
    boost::filesystem::path pathConfigFile(filename);
    if (!pathConfigFile.is_complete())
        pathConfigFile = GetCoinToolDir() / pathConfigFile;

    return pathConfigFile;
}

extern void InterpretNegativeSetting(std::string& strKey, std::string& strValue);
void ReadFile(map<string, string>& mapSettingsRet,
              map<string, vector<string> >& mapMultiSettingsRet,
              const std::string & strfile)
{
    boost::filesystem::ifstream streamFile(GetFile(strfile));
    if (!streamFile.good()){
        // Create empty ulord.conf if it does not excist
        FILE* configFile = fopen(GetFile(strfile).c_str(), "a");
        if (configFile != NULL)
            fclose(configFile);
        return; // Nothing to read, so just return
    }

    set<string> setOptions;
    setOptions.insert("*");

    for (boost::program_options::detail::config_file_iterator it(streamFile, setOptions), end; it != end; ++it)
    {
        // Don't overwrite existing settings so command line settings override ulord.conf
        string strKey = string("-") + it->string_key;
        string strValue = it->value[0];
        InterpretNegativeSetting(strKey, strValue);
        if (mapSettingsRet.count(strKey) == 0)
            mapSettingsRet[strKey] = strValue;
        mapMultiSettingsRet[strKey].push_back(strValue);
    }
}

bool AddOneNode(const string & strNode)
{
	CAddress addr;
	return OpenNetworkConnection(addr, NULL, strNode.c_str());
}

bool GetKeysFromSecret(std::string strSecret, CKey& keyRet, CPubKey& pubkeyRet)
{
    CBitcoinSecret vchSecret;

    if(!vchSecret.SetString(strSecret)) return false;

    keyRet = vchSecret.GetKey();
    pubkeyRet = keyRet.GetPubKey();

    return true;
}

