#include "utils.h"
#include "protocol.h"
#include "net.h"
#include "addrman.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>

#include <stdarg.h>

using namespace std;

static std::string GetTimestampStr(const std::string &str, bool *fStartedNewLine)
{
    string strStamped;

    if (!fLogTimestamps)
        return str;

    if (*fStartedNewLine) {
        int64_t nTimeMicros = GetLogTimeMicros();
        strStamped = DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nTimeMicros/1000000);
        if (fLogTimeMicros)
            strStamped += strprintf(".%06d", nTimeMicros%1000000);
        strStamped += ' ' + str;
    } else
        strStamped = str;

    return strStamped;
}
static std::string GetThreadNameStr(const std::string &str, bool *fStartedNewLine)
{
    string strThreadLogged;

    if (!fLogThreadNames)
        return str;

    std::string strThreadName = GetThreadName();

    if (*fStartedNewLine)
        strThreadLogged = strprintf("%16s | %s", strThreadName.c_str(), str.c_str());
    else
        strThreadLogged = str;

    return strThreadLogged;
}

int PrintStr(const std::string &str)
{
    int ret = 0; // Returns total number of characters written
    static bool fStartedNewLine = true;

    std::string strThreadLogged = GetThreadNameStr(str, &fStartedNewLine);
    std::string strTimestamped = GetTimestampStr(strThreadLogged, &fStartedNewLine);

    if (!str.empty() && str[str.size()-1] == '\n')
        fStartedNewLine = true;
    else
        fStartedNewLine = false;

    // print to console
    ret = fwrite(strTimestamped.data(), 1, strTimestamped.size(), stdout);
    fflush(stdout);
    
    return ret;
}

bool showerror(const char* format, ...)
{
    char buf[100];
    memset(buf, 0, 100);
    va_list args;
    va_start(args,format);
    vsprintf(buf,format,args);
    va_end(args);

    PrintStr(std::string("ERROR: ") + string(buf, buf + strlen(buf)) + "\n");
    return false;
}

boost::filesystem::path GetCoinToolDir()
{
    namespace fs = boost::filesystem;
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\CoinTools
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\CoinTools
    // Mac: ~/Library/Application Support/CoinTools
    // Unix: ~/.cointools
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "CoinTools";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    return pathRet / "Library/Application Support/CoinTools";
#else
    // Unix
    return pathRet / ".cointools";
#endif
#endif
}

void SetFilePath(const std::string & filename)
{
    mapArgs["-datadir"] = GetCoinToolDir().string();

    boost::filesystem::path pathFile (GetCoinToolDir() / filename);
    mapArgs["-conf"] = pathFile.string();

    cout << "Using config file " << GetConfigFile().string() << endl;
}

void SetParams()
{
    if(GetBoolArg("-testnet", false))
        SelectParams(CBaseChainParams::TESTNET);
    else
        SelectParams(CBaseChainParams::MAIN);
}

bool AddOneNode(const string & strNode, bool fConnectToMasternode)
{
	//CAddress addr;
	//return OpenNetworkConnection(addr, NULL, strNode.c_str());
    CService destaddr = CService(strNode);
    bool proxyConnectionFailed = false;
    SOCKET hSocket;
    if(ConnectSocket(destaddr, hSocket, DEFAULT_CONNECT_TIMEOUT, &proxyConnectionFailed))
    {
        if (!IsSelectableSocket(hSocket)) {
            cout << "Cannot create connection: non-selectable socket created (fd >= FD_SETSIZE ?)" << endl;
            CloseSocket(hSocket);
            return false;
        }
        addrman.Attempt(destaddr);

        // Add node
        CNode* pnode = new CNode(hSocket, (CAddress)destaddr, "", false, true);

        pnode->nTimeConnected = GetTime();
		pnode->nVersion = PROTOCOL_VERSION;
        if(fConnectToMasternode) {
            pnode->AddRef();
            pnode->fMasternode = true;
        }

        LOCK(cs_vNodes);
        vNodes.push_back(pnode);
        return true;
    }
    cout << "ConnectSocket failed!" << endl;
    return false;
}

void FreeNode()
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        pnode->fDisconnect = true;
        CloseSocket(pnode->hSocket);
        //vNodes->erase(pnode);
        delete pnode;
    }
    vNodes.clear();
}

bool GetKeysFromSecret(std::string strSecret, CKey& keyRet, CPubKey& pubkeyRet)
{
    CBitcoinSecret vchSecret;

    if(!vchSecret.SetString(strSecret)) return showerror("GetKeysFromSecret:SetString error");

    keyRet = vchSecret.GetKey();
    pubkeyRet = keyRet.GetPubKey();

    return true;
}
