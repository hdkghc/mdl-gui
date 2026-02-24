/*

mdl_gui.cpp ( automatic Movie DownLoader GUI version 3.2 ) 

Copyright (C) 2026  ZnBkCu(aka. hdkghc)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

---

版权所有 (C) 2026  ZnBkCu(又名 hdkghc)

本程序是自由软件：你可以依据自由软件基金会发布的 GNU 通用公共许可证第3版
（或你选择的任何后续版本）的条款重新分发和/或修改本程序。

本程序的发布是希望它能发挥作用，但**不提供任何担保**；甚至不保证适销性
或特定用途的适用性。详情请参阅 GNU 通用公共许可证。

你应已收到本程序附带的 GNU 通用公共许可证副本。
如果没有，请查阅 <https://www.gnu.org/licenses/>。

完整的GNU通用公共许可证副本请见仓库根目录的LICENSE文件。

*/


// Compile: g++ mdl_gui.cpp -o mdl_gui.exe -std=c++17 -lstdc++fs -static-libgcc -static-libstdc++ -lwininet -ldbghelp -lcomctl32 -lgdi32 -Os -s
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <locale>
#include <wininet.h>
#include <regex>
#include <vector>
#include <ctime>
#include <sstream>
#include <csignal>
#include <DbgHelp.h>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>
#include <streambuf>
#include <mutex>
#include <shellapi.h>
#include <richedit.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "comctl32.lib")

using namespace std;
namespace fs = filesystem;

// 调试开关
bool g_debugMode = false;

// 使用 curl
bool g_useCurl = false;

// 影片信息
struct MovieInfo {
	string title;       // 影片标题
	string detailUrl;   // 详情页链接
	string playUrl;     // 播放页链接
};

HWND 				hWndMain 		= NULL;
HWND 				hEditLog 		= NULL;		// 日志
HWND 				hEditProject 	= NULL;		// 项目输入框
HWND 				hEditMovieName 	= NULL;		// 电影名输入框
HWND 				hListMovies 	= NULL;		// 电影列表
HWND				hBtnSync		= NULL;		// 同步按钮
HWND 				hBtnSearch 		= NULL;		// 搜索按钮
HWND 				hBtnDownload 	= NULL;		// 下载按钮
HWND 				hBtnDetail 		= NULL;		// 详情按钮
HWND 				hProgressBar 	= NULL;		// 进度
HWND 				hProgressBarTS 	= NULL;  	// TS分片下载进度条
HWND				hChkCurl		= NULL;		// curl 作为下载器复选框

string 				g_projectName;
string 				g_tempFolder;  				// 随机十六进制命名的临时文件夹
string 				g_selectedM3u8Url;
vector<MovieInfo> 	g_movieList;
const int 			MAX_RETRY 		= 3;
const int 			RETRY_DELAY 	= 2000;
const int			Max				= 2000;

// 日志颜色
enum LogColor {
	COLOR_INFO 	= 0x0000FF,
	COLOR_DEBUG = 0xFF7F00,
	COLOR_WARN 	= 0xFFFF00,
	COLOR_ERROR = 0xFF0000,
	COLOR_FATAL = 0xFF00FF
};

// 互斥锁
mutex g_logMutex;

string domains[] = {
	"tvdy.cc",
	"tvdy.xyz",
	"tvdy2.com"
};
int nDomain = 3;
string domain;

string header = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

string license = "\nmdl_gui  Copyright (C) 2026  ZnBkCu(aka. hdkghc)\nThis program comes with ABSOLUTELY NO WARRANTY;\nThis is free software, and you are welcome to redistribute it under certain conditions";

// 随机十六进制字符串
string generateRandomHexString (int length = 16) {
	random_device rd;
	mt19937 gen (rd());
	uniform_int_distribution<> dis (0, 15);

	const char *hexChars = "0123456789ABCDEF";
	stringstream ss;

	for (int i = 0; i < length; ++i) {
		ss << hexChars[dis (gen)];
	}

	return ss.str();
}

// 删除空白字符
void EatTail (string &s) {
	while (!s.empty()) {
		char c = s.back();

		if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
			s.pop_back();
		} else {
			break;
		}
	}
}

// 获取当前时间戳（YYYY-MM-DD HH:MM:SS）
string getTimestamp() {
	time_t now = time (nullptr);
	tm tm_now;
	localtime_s (&tm_now, &now);
	char buf[32];
	sprintf (buf,
	         "%04d-%02d-%02d %02d:%02d:%02d",
	         tm_now.tm_year + 1900,
	         tm_now.tm_mon + 1,
	         tm_now.tm_mday,
	         tm_now.tm_hour,
	         tm_now.tm_min,
	         tm_now.tm_sec
	        );
	return string (buf);
}

// 设置文本颜色
//void SetTextColor(HWND hEdit, COLORREF color) {
//    CHARFORMAT2 cf 	= {0};
//    cf.cbSize 		= sizeof(CHARFORMAT2);
//    cf.dwMask 		= CFM_COLOR;
//    cf.crTextColor 	= color;
//    // 必须先选中插入位置，再设置格式
//    SendMessage(hEdit, EM_SETSEL, 			(WPARAM)-1, 	(LPARAM)-1);
//    SendMessage(hEdit, EM_SETCHARFORMAT, 	SCF_SELECTION, 	(LPARAM)&cf);
//    // 恢复光标到末尾
//    int len = GetWindowTextLengthA(hEdit);
//    SendMessage(hEdit, EM_SETSEL, 			len, 			len);
//}

// GUI日志输出函数（支持彩色，修复版）
void LogToEdit (const string &level, const string &msg, COLORREF color = 0, const string &func = "---") {
	lock_guard<mutex> lock (g_logMutex);

	if (!hEditLog) return;

	// 根据日志级别设置默认颜色
	if (color == 0) {
		if (level == "INFO") 	color = COLOR_INFO;
		else if (level == "DEBUG") 	color = COLOR_DEBUG;
		else if (level == "WARN") 	color = COLOR_WARN;
		else if (level == "ERROR") 	color = COLOR_ERROR;
		else if (level == "FATAL") 	color = COLOR_FATAL;
		else color = 0xFF7F00; // 默认
	}

	if(level != "DEBUG")
	{
		string logMsg = "[" + getTimestamp() + "] [" + level + "] " + msg + "\r\n";
		
		// 1. 将光标移到末尾
		int len = GetWindowTextLengthA (hEditLog);
		SendMessageA (hEditLog, EM_SETSEL, len, len);
		
		// 2. 设置颜色（关键：先选位置再设颜色）
//    SetTextColor(hEditLog, color);
		
		// 3. 插入文本
		SendMessageA (hEditLog, EM_REPLACESEL, FALSE, (LPARAM)logMsg.c_str());
		
		// 4. 滚动到最后一行
		SendMessageA (hEditLog, EM_SCROLLCARET, 0, 0);
	}

	// 调试模式下同时输出到控制台
	if (g_debugMode) {
		BYTE r = (color & 0xff0000) >> 16;
		BYTE g = (color & 0x00ff00) >> 8;
		BYTE b = (color & 0x0000ff) >> 0;
		printf (
		    "\033[0m\033[38;2;214;255;72m[%s]\033[38;2;255;127;0m\033[1m<%s>\033[0m\033[38;2;%d;%d;%dm[%s]\033[0m%s\033[0m\n",
		    getTimestamp().data(),
			func.data(),
		    r, g, b,
		    level.data(),
		    msg.data()
		);
	}
}

#define LOG_INFOR(msg) 					LogToEdit("INFO", msg, COLOR_INFO, __func__)
#define LOG_DEBUG(msg) if(g_debugMode) 	LogToEdit("DEBUG", msg, COLOR_DEBUG, __func__)
#define LOG_WARNG(msg) 					LogToEdit("WARN", msg, COLOR_WARN, __func__)
#define LOG_ERROR(msg) 					LogToEdit("ERROR", msg, COLOR_ERROR, __func__)
#define LOG_FATAL(msg) \
	do{ \
		string fatalMsg = msg; \
		LogToEdit("FATAL", fatalMsg, COLOR_FATAL, __func__); \
		MessageBoxA(hWndMain, fatalMsg.c_str(), "Fatal Error", MB_ICONERROR); \
	}while(0)

//class LogRedirectBuffer : public streambuf {
//protected:
//    int overflow(int c) override {
//        if (c != EOF) {
//            static string buffer;
//            if (c == '\n') {
//                LogToEdit("CONSOLE", buffer, 0x808080);
//                buffer.clear();
//            } else {
//                buffer += static_cast<char>(c);
//            }
//        }
//        return c;
//    }
//
//    int sync() override {
//        return 0;
//    }
//} g_coutBuffer, g_cerrBuffer;

// UTF-8转ANSI
string utf8ToAnsi (const string &utf8Str) {
	LOG_DEBUG ("UTF-8转ANSI，原始输入长度: " + to_string (utf8Str.size()));

	// 先将UTF-8转为宽字符
	int wideLen = MultiByteToWideChar (CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.length(), NULL, 0);

	if (wideLen <= 0) {
		LOG_ERROR ("UTF-8转宽字符失败，错误码: " + to_string (GetLastError()));
		return utf8Str;
	}

	wchar_t *wideStr = new wchar_t[wideLen + 1];
	MultiByteToWideChar (CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.length(), wideStr, wideLen);
	wideStr[wideLen] = L'\0';

	// 再将宽字符转为ANSI
	int ansiLen = WideCharToMultiByte (CP_ACP, 0, wideStr, wideLen, NULL, 0, NULL, NULL);

	if (ansiLen <= 0) {
		LOG_ERROR ("宽字符转ANSI失败，错误码: " + to_string (GetLastError()));
		delete[] wideStr;
		return utf8Str;
	}

	char *ansiStr = new char[ansiLen + 1];
	WideCharToMultiByte (CP_ACP, 0, wideStr, wideLen, ansiStr, ansiLen, NULL, NULL);
	ansiStr[ansiLen] = '\0';

	string result (ansiStr);
	delete[] wideStr;
	delete[] ansiStr;

	LOG_DEBUG ("UTF-8转ANSI结果: [" + result + "]");
	return result;
}

// ANSI转UTF-8
string ansiToUtf8 (const string &ansiStr) {
	LOG_DEBUG ("ANSI转UTF-8，原始输入: [" + ansiStr + "]");

	// ANSI字符串转宽字符
	int wideLen = MultiByteToWideChar (CP_ACP, 0, ansiStr.c_str(), (int)ansiStr.length(), NULL, 0);

	if (wideLen <= 0) {
		LOG_ERROR ("ANSI转宽字符失败，错误码: " + to_string (GetLastError()));
		return ansiStr;
	}

	wchar_t *wideStr = new wchar_t[wideLen + 1];
	MultiByteToWideChar (CP_ACP, 0, ansiStr.c_str(), (int)ansiStr.length(), wideStr, wideLen);
	wideStr[wideLen] = L'\0';

	// 宽字符转UTF-8
	int utf8Len = WideCharToMultiByte (CP_UTF8, 0, wideStr, wideLen, NULL, 0, NULL, NULL);

	if (utf8Len <= 0) {
		LOG_ERROR ("宽字符转UTF-8失败，错误码: " + to_string (GetLastError()));
		delete[] wideStr;
		return ansiStr;
	}

	char *utf8Str = new char[utf8Len + 1];
	WideCharToMultiByte (CP_UTF8, 0, wideStr, wideLen, utf8Str, utf8Len, NULL, NULL);
	utf8Str[utf8Len] = '\0';

	string result (utf8Str);
	delete[] wideStr;
	delete[] utf8Str;

	return result;
}

// 替换
void replace (string &str, char oldChar, char newChar) {
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] == oldChar) {
			str[i] = newChar;
		}
	}
}

// 保存字符串到本地文件
void saveStringToFile (const string& content, const string& filename) {
	ofstream file (filename, ios::out | ios::binary);

	if (file.is_open()) {
		file.write (content.c_str(), content.size());
		file.close();
		LOG_INFOR ("调试文件已保存到: [" + filename + "]");
	} else {
		LOG_ERROR ("无法保存调试文件: [" + filename + "]");
	}
}

// 提取URL的基础路径
string getBaseUrl (const string &url) {
	LOG_DEBUG ("输入URL: [" + url + "]");
	size_t lastSlash = url.find_last_of ("/");

	if (lastSlash == string::npos) {
		LOG_WARNG ("未找到路径分隔符，返回空字符串");
		return "";
	}

	string base = url.substr (0, lastSlash + 1);
	LOG_DEBUG ("提取基础路径: [" + base + "]");
	return base;
}

// 从TS完整URL中提取文件名
string getTsFileName (const string &tsUrl) {
	LOG_DEBUG ("输入TS URL: [" + tsUrl + "]");
	size_t lastSlash = tsUrl.find_last_of ("/");
	string fileName = (lastSlash == string::npos) ? tsUrl : tsUrl.substr (lastSlash + 1);
	LOG_DEBUG ("提取文件名: [" + fileName + "]");
	return fileName;
}

// 检测指定目录下是否有.aria2残留文件
bool hasAria2Residue (const string &dir) {
	LOG_DEBUG ("检测目录: [" + dir + "]");

	try {
		fs::path dirPath = fs::u8path (dir);

		if (!fs::exists (dirPath)) {
			LOG_DEBUG ("目录不存在，返回false");
			return false;
		}

		for (const auto& entry : fs::directory_iterator (dirPath)) {
			if (entry.path().extension() == fs::u8path (".aria2")) {
				LOG_WARNG ("检测到残留文件: [" + entry.path().filename().u8string() + "]");
				return true;
			}
		}

		LOG_DEBUG ("未检测到残留文件");
	} catch (const fs::filesystem_error& e) {
		LOG_ERROR ("检测失败: " + string (e.what()));
	}

	return false;
}

// 删除指定目录下的.aria2残留文件
void deleteAria2Residue (const string &dir) {
	LOG_DEBUG ("清理目录: [" + dir + "]");

	try {
		fs::path dirPath = fs::u8path (dir);

		if (!fs::exists (dirPath)) {
			LOG_DEBUG ("目录不存在，直接返回");
			return;
		}

		for (const auto& entry : fs::directory_iterator (dirPath)) {
			if (entry.path().extension() == fs::u8path (".aria2")) {
				fs::remove (entry.path());
				LOG_INFOR ("已删除残留文件: [" + entry.path().filename().u8string() + "]");
			}
		}
	} catch (const fs::filesystem_error& e) {
		LOG_ERROR ("清理失败: " + string (e.what()));
	}
}

// 下载单个文件（如m3u8文件）+ 重试逻辑
string downloadSingle (string file, string downloader = "aria2c.exe --check-certificate=false") {
	LOG_INFOR ("开始下载单个文件: [" + file + "]");
	size_t lastSlash = file.find_last_of ("/\\");
	string fileName = (lastSlash == string::npos) ? file : file.substr (lastSlash + 1);
	string filePath = ".\\" + g_tempFolder + "\\" + fileName;

	LOG_DEBUG ("保存路径: [" + filePath + "]");
	int retryCount = 0;
	bool downloadSuccess = false;

	if (g_useCurl) {
		downloader = "curl.exe";
	}

	while (retryCount < MAX_RETRY && !downloadSuccess) {
		string cmdl = downloader + " \"" + file + "\" -o \"" + filePath + "\"";
		LOG_INFOR ("【第" + to_string (retryCount + 1) + "次下载】执行命令: [" + cmdl + "]");

		int ret = system (cmdl.c_str());
		LOG_DEBUG ("命令执行返回值: " + to_string (ret));

		fs::path filePathObj = fs::u8path (filePath);
		fs::path aria2PathObj = filePathObj;
		aria2PathObj += fs::u8path (".aria2");

		if (fs::exists (filePathObj) && !fs::exists (aria2PathObj)) {
			downloadSuccess = true;
			LOG_INFOR ("文件下载成功: [" + filePath + "]");
		} else {
			retryCount++;
			LOG_WARNG ("文件下载失败，重试次数: " + to_string (retryCount));

			if (retryCount < MAX_RETRY) {
				LOG_WARNG ("等待" + to_string (RETRY_DELAY / 1000) + "秒后重试...");
				Sleep (RETRY_DELAY);
				deleteAria2Residue (".\\" + g_tempFolder);
			}
		}
	}

	if (!downloadSuccess) {
		LOG_ERROR ("文件下载失败（已重试" + to_string (MAX_RETRY) + "次）: [" + file + "]");
		throw runtime_error ("Single file download failed: " + file);
	}

	return filePath;
}

// 统计list.txt中的TS文件总数
int countTotalTSSegments (const string &listPath) {
	ifstream ifs (listPath);

	if (!ifs.is_open()) {
		LOG_ERROR ("无法打开分片列表文件: [" + listPath + "]");
		return 0;
	}

	int count = 0;
	string line;

	while (getline (ifs, line)) {
		EatTail (line);

		if (!line.empty() && (line.find ("http://") == 0 || line.find ("https://") == 0)) {
			count++;
		}
	}

	ifs.close();
	LOG_INFOR ("TS分片总数: " + to_string (count));
	return count;
}

// 统计已下载的TS文件数
int countDownloadedTSSegments (const string &saveDir) {
	int count = 0;

	try {
		fs::path dirPath = fs::u8path (saveDir);

		if (!fs::exists (dirPath)) {
			return 0;
		}

		for (const auto& entry : fs::directory_iterator (dirPath)) {
			string ext = entry.path().extension().u8string();

			// 只统计.ts文件，排除.aria2和其他临时文件
			if (ext == ".ts" && !fs::exists (entry.path().u8string() + ".aria2")) {
				count++;
			}
		}
	} catch (const fs::filesystem_error& e) {
		LOG_ERROR ("统计已下载TS文件失败: " + string (e.what()));
	}

	return count;
}

// 批量下载TS分片
int downloadTsSegments (const string &listPath, const string &saveDir, string downloader = "aria2c.exe --check-certificate=false") {
	LOG_INFOR ("开始批量下载TS分片，列表文件: [" + listPath + "]，保存目录: [" + saveDir + "]");

	// 1. 获取TS分片总数
	int totalTS = countTotalTSSegments (listPath);

	if (totalTS == 0) {
		LOG_ERROR ("未找到有效TS分片！");
		return -1;
	}

	// 2. 重置TS进度条
	SendMessageA (hProgressBarTS, PBM_SETPOS, 0, 0);

	int retryCount = 0;
	bool downloadSuccess = false;

	if (!g_useCurl) {
		while (retryCount < MAX_RETRY && !downloadSuccess) {
			// 启动aria2c下载（异步执行，避免阻塞进度更新）
			string cmdl = downloader + " -x 16 -s 16 -c -i \"" + listPath + "\" -d \"" + saveDir + "\"";
			LOG_INFOR ("【第" + to_string (retryCount + 1) + "次批量下载】执行命令: [" + cmdl + "]");
			
			// 创建子进程执行下载，避免阻塞主线程
			STARTUPINFOA si = {0};
			PROCESS_INFORMATION pi = {0};
			si.cb = sizeof (STARTUPINFOA);
			
			if (!CreateProcessA (NULL, (char * )cmdl.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
				LOG_ERROR ("创建下载进程失败，错误码: " + to_string (GetLastError()));
				retryCount++;
				continue;
			}
			
			// 3. 实时监控下载进度
			DWORD waitResult;
			
			do {
				// 等待1秒，避免频繁检测
				waitResult = WaitForSingleObject (pi.hProcess, 1000);
				
				// 统计已下载的TS文件数
				int downloaded = countDownloadedTSSegments (saveDir);
				int progress = (int) (((double)downloaded / totalTS) * Max);
				
				// 更新TS进度条
				SendMessageA (hProgressBarTS, PBM_SETPOS, progress, 0);
				
				// 日志输出进度
				LOG_DEBUG ("TS下载进度: " + to_string (downloaded) + "/" + to_string (totalTS) + " (" + to_string ((((double)downloaded / totalTS) * 100)) + "%)");
				
			} while (waitResult == WAIT_TIMEOUT);
			
			// 关闭进程句柄
			CloseHandle (pi.hProcess);
			CloseHandle (pi.hThread);
			
			// 检查下载结果
			if (!hasAria2Residue (saveDir)) {
				downloadSuccess = true;
				LOG_INFOR ("分片批量下载成功！");
				// 进度条拉满
				SendMessageA (hProgressBarTS, PBM_SETPOS, Max, 0);
			} else {
				retryCount++;
				LOG_WARNG ("分片下载失败，重试次数: " + to_string (retryCount));
				
				if (retryCount < MAX_RETRY) {
					LOG_WARNG ("等待" + to_string (RETRY_DELAY / 1000) + "秒后重试...");
					Sleep (RETRY_DELAY);
					deleteAria2Residue (saveDir);
					// 重置TS进度条
					SendMessageA (hProgressBarTS, PBM_SETPOS, 0, 0);
				}
			}
		}
	} else {
		downloadSuccess = true;
		vector<string> list;
		ifstream ifs;
		ifs.open (listPath);
		string line;

		while (getline (ifs, line)) {
			EatTail (line);
			list.push_back (line);
		}

		int i = 0;

		for (auto u : list) {
			string cmdl = "curl.exe \"" + u + "\" -o \"" + saveDir + "\\" + u.substr (u.find_last_of ('/') + 1) + "\"";
			LOG_DEBUG("Command [" + cmdl + "]");
			system (cmdl.data());
			++i;
			int progress = (int) ((double) (i) / totalTS * Max);

			// 更新TS进度条
			SendMessageA (hProgressBarTS, PBM_SETPOS, progress, 0);
		}
	}

	if (!downloadSuccess) {
		LOG_ERROR ("分片下载失败（已重试" + to_string (MAX_RETRY) + "次）");
		// 失败时重置进度条
		SendMessageA (hProgressBarTS, PBM_SETPOS, 0, 0);
		return -1;
	}

//	// 下载完成后延迟1秒再重置（让用户看到100%进度）
//	Sleep (1000);
//	SendMessageA (hProgressBarTS, PBM_SETPOS, 0, 0);
	return 0;
}

// 下载 mixed.m3u8 文件
string downloadMixed (string &m3u8File, const string &m3u8Url, string downloader = "aria2c.exe --check-certificate=false") {
	LOG_INFOR ("开始解析M3U8文件: [" + m3u8File + "]");
	ifstream ifs (m3u8File);

	if (!ifs.is_open()) {
		LOG_ERROR ("无法打开m3u8文件: [" + m3u8File + "]");
		return "";
	}

	string baseUrl = getBaseUrl (m3u8Url);
	LOG_INFOR ("M3U8基础路径: [" + baseUrl + "]");

	string listPath = ".\\" + g_tempFolder + "\\list.txt";
	ofstream ofs (listPath);

	if (!ofs.is_open()) {
		ifs.close();
		LOG_ERROR ("无法创建list.txt文件: [" + listPath + "]");
		return "";
	}

	string line, mixedpath;

	while (getline (ifs, line)) {
		EatTail (line);
		LOG_DEBUG ("读取M3U8行: [" + line + "]");

		if (line.empty() || line[0] == '#') continue;

		string tsUrl = line;

		if (tsUrl.find ("http://") != 0 && tsUrl.find ("https://") != 0) {
			tsUrl = baseUrl + tsUrl;
			LOG_DEBUG ("补全URL: [" + tsUrl + "]");
		}

		m3u8File = getTsFileName (tsUrl);
		mixedpath = tsUrl.substr (0, tsUrl.find_last_of ("/") + 1);
		ofs << tsUrl << endl;
		LOG_DEBUG ("主分片文件名: [" + m3u8File + "]");
	}

	ifs.close();
	ofs.close();

	string saveDir = ".\\" + g_tempFolder;

	if (downloadTsSegments (listPath, saveDir, downloader) != 0) {
		return "";
	}

	m3u8File = ".\\" + g_tempFolder + "\\" + m3u8File;
	return mixedpath;
}

// 下载m3u8中的分片文件
int downloadM3u8 (string m3u8File, const string &m3u8Url, string downloader = "aria2c.exe --check-certificate=false") {
	LOG_INFOR ("开始解析M3U8文件: [" + m3u8File + "]");
	ifstream ifs (m3u8File);

	if (!ifs.is_open()) {
		LOG_ERROR ("无法打开m3u8文件: [" + m3u8File + "]");
		return -1;
	}

	string baseUrl = getBaseUrl (m3u8Url);
	LOG_INFOR ("M3U8基础路径: [" + baseUrl + "]");

	string listPath = ".\\" + g_tempFolder + "\\list.txt";
	ofstream ofs (listPath);

	if (!ofs.is_open()) {
		ifs.close();
		LOG_ERROR ("无法创建list.txt文件: [" + listPath + "]");
		return -1;
	}

	int validCnt = 0;
	size_t mainFileNameLen = 0;
	string line;
	int filteredCnt = 0;

	while (getline (ifs, line)) {
		EatTail (line);
		LOG_DEBUG ("读取M3U8行: [" + line + "]");

		if (line.empty() || line[0] == '#') continue;

		string tsUrl = line;

		if (tsUrl.find ("http://") != 0 && tsUrl.find ("https://") != 0) {
			tsUrl = baseUrl + tsUrl;
			LOG_DEBUG ("补全TS URL: [" + tsUrl + "]");
		}

		string tsFileName = getTsFileName (tsUrl);
		size_t fileNameLen = tsFileName.size();

		if (validCnt == 0) {
			mainFileNameLen = fileNameLen;
			ofs << tsUrl << endl;
			validCnt++;
			LOG_DEBUG ("主分片文件名: [" + tsFileName + "] | 长度基准: " + to_string (mainFileNameLen));
		} else {
			if (fileNameLen == mainFileNameLen) {
				ofs << tsUrl << endl;
				validCnt++;
			} else {
				filteredCnt++;
				LOG_WARNG ("过滤广告分片（文件名长度" + to_string (fileNameLen) + "!=基准" + to_string (mainFileNameLen) + "）: [" + tsFileName + "]");
			}
		}
	}

	ifs.close();
	ofs.close();

	if (validCnt == 0) {
		LOG_ERROR ("M3U8中未找到有效TS分片！");
		return -1;
	}

	LOG_INFOR ("\n分片过滤完成：共找到" + to_string (validCnt + filteredCnt) + "个分片，保留" + to_string (validCnt) + "个，过滤" + to_string (filteredCnt) + "个广告分片\n");

	string saveDir = ".\\" + g_tempFolder;

	if (downloadTsSegments (listPath, saveDir, downloader) != 0) {
		return -1;
	}

	return 0;
}

// FFmpeg拼接函数
int vidcat (string ff = "ffmpeg.exe") {
	LOG_INFOR ("开始拼接TS文件");
	string listPath = ".\\" + g_tempFolder + "\\list.txt";
	string seqPath = ".\\" + g_tempFolder + "\\seq.txt";

	ifstream ifs (listPath);

	if (!ifs.is_open()) {
		LOG_ERROR ("无法打开list.txt文件: [" + listPath + "]");
		return -1;
	}

	ofstream ofs (seqPath);

	if (!ofs.is_open()) {
		ifs.close();
		LOG_ERROR ("无法创建seq.txt文件: [" + seqPath + "]");
		return -1;
	}

	string line;

	while (getline (ifs, line)) {
		EatTail (line);

		if (line.empty()) continue;

		string segName = getTsFileName (line);
		ofs << "file '" << segName << "'" << endl;
		LOG_DEBUG ("添加分片到拼接列表: [" + segName + "]");
	}

	ifs.close();
	ofs.close();

	string outputFile = g_projectName + ".mp4";
	string ffmpegCmd = ff + " -f concat -safe 0 -i \"" + seqPath + "\" -c copy -y \"" + outputFile + "\"";

	LOG_INFOR ("执行FFmpeg命令: [" + ffmpegCmd + "]");
	int ret = system (ffmpegCmd.c_str());
	LOG_DEBUG ("FFmpeg命令返回值: " + to_string (ret));

	ifstream checkOutput (outputFile);

	if (!checkOutput.is_open()) {
		LOG_ERROR ("FFmpeg拼接失败，未生成输出文件: [" + outputFile + "]");
		return -1;
	}

	checkOutput.close();

	LOG_INFOR ("拼接完成！输出文件: [" + outputFile + "]");
	return 0;
}

// URL编码（UTF-8）
string urlEncode (const string &str) {
	LOG_DEBUG ("URL编码，原始输入: [" + str + "]");
	// 转换为UTF-8
	string utf8Str = ansiToUtf8 (str);

	string encoded;

	for (unsigned char c : utf8Str) {
		if (isdigit (c) || isalpha (c) || c == '-' || c == '_' || c == '.' || c == '~') {
			encoded += c;
		} else {
			char buf[10];
			sprintf (buf, "%%%02X", c);
			encoded += buf;
		}
	}

	LOG_DEBUG ("URL编码最终结果: [" + encoded + "]");
	return encoded;
}

// 获取网页内容
string httpGet (const string &url, bool convertToAnsi = true) {
	LOG_INFOR ("发送HTTP GET请求: [" + url + "]");
	HINTERNET hInternet = InternetOpenA (header.data(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if (!hInternet) {
		DWORD err = GetLastError();
		LOG_ERROR ("InternetOpen失败，错误码: " + to_string (err));
		return "";
	}

	HINTERNET hConnect = InternetOpenUrlA (hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);

	if (!hConnect) {
		DWORD err = GetLastError();
		LOG_ERROR ("InternetOpenUrl失败，错误码: " + to_string (err));
		InternetCloseHandle (hInternet);
		return "";
	}

	char buffer[4096];
	DWORD bytesRead;
	string content;

	while (InternetReadFile (hConnect, buffer, sizeof (buffer) - 1, &bytesRead) && bytesRead > 0) {
		buffer[bytesRead] = 0;
		content += buffer;
		LOG_DEBUG ("读取HTTP数据，本次长度: " + to_string (bytesRead) + "，累计长度: " + to_string (content.size()));
	}

	InternetCloseHandle (hConnect);
	InternetCloseHandle (hInternet);

	// 转换编码（UTF-8转ANSI）
	if (convertToAnsi) {
		content = utf8ToAnsi (content);
	}

	LOG_DEBUG ("HTTP响应总长度(转换后): " + to_string (content.size()));
	return content;
}

// 获取纯原始HTTP响应（不做任何编码转换）
string httpGetRaw (const string &url) {
	HINTERNET hInternet = InternetOpenA (header.data(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if (!hInternet) {
		LOG_ERROR ("InternetOpen失败，错误码: " + to_string (GetLastError()));
		return "";
	}

	HINTERNET hConnect = InternetOpenUrlA (hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);

	if (!hConnect) {
		LOG_ERROR ("InternetOpenUrl失败，URL: [" + url + "]，错误码: " + to_string (GetLastError()));
		InternetCloseHandle (hInternet);
		return "";
	}

	string response;
	char buffer[4096];
	DWORD bytesRead = 0;

	while (InternetReadFile (hConnect, buffer, sizeof (buffer) - 1, &bytesRead) && bytesRead > 0) {
		buffer[bytesRead] = '\0';
		response.append (buffer, bytesRead); // 直接追加原始字节，不做任何转换
	}

	InternetCloseHandle (hConnect);
	InternetCloseHandle (hInternet);

	LOG_DEBUG ("httpGetRaw获取原始数据长度: " + to_string (response.size()));
	return response;
}

// unescape解码
string unescape (const string &str) {
	LOG_DEBUG ("开始unescape解码，输入: [" + str + "]");
	string decoded;
	size_t i = 0;

	while (i < str.size()) {
		if (str[i] == '%' && i + 2 < str.size()) {
			// 解析%XX格式的转义字符
			char hex[3] = {str[i + 1], str[i + 2], '\0'};
			char c = (char)strtol (hex, nullptr, 16);
			decoded += c;
			i += 3;
		} else if (str[i] == '+') {
			decoded += ' ';
			i++;
		} else {
			decoded += str[i];
			i++;
		}
	}

	LOG_DEBUG ("unescape解码完成，输出: [" + decoded + "]");
	return decoded;
}

// Base64解码
string base64Decode (const string &encoded) {
	LOG_DEBUG ("开始Base64解码，输入长度: " + to_string (encoded.size()));
	static const string base64Chars =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789+/";

	string decoded;
	vector<int> vec (4, 0);

	int i = 0;
	int j = 0;
	unsigned char charArray3[3];
	unsigned char charArray4[4];

	for (char c : encoded) {
		if (c == '=') break;

		if (base64Chars.find (c) == string::npos) continue;

		charArray4[i++] = c;

		if (i == 4) {
			for (i = 0; i < 4; i++) {
				vec[i] = base64Chars.find (charArray4[i]);
			}

			charArray3[0] = (vec[0] << 2) + ((vec[1] & 0x30) >> 4);
			charArray3[1] = ((vec[1] & 0xf) << 4) + ((vec[2] & 0x3c) >> 2);
			charArray3[2] = ((vec[2] & 0x3) << 6) + vec[3];

			for (i = 0; i < 3; i++) {
				decoded += charArray3[i];
			}

			i = 0;
		}
	}

	if (i > 0) {
		for (j = i; j < 4; j++) {
			vec[j] = 0;
		}

		for (j = 0; j < 4; j++) {
			charArray4[j] = base64Chars[vec[j]];
		}

		for (j = 0; j < 4; j++) {
			vec[j] = base64Chars.find (charArray4[j]);
		}

		charArray3[0] = (vec[0] << 2) + ((vec[1] & 0x30) >> 4);
		charArray3[1] = ((vec[1] & 0xf) << 4) + ((vec[2] & 0x3c) >> 2);

		for (j = 0; j < i - 1; j++) {
			decoded += charArray3[j];
		}
	}

	LOG_DEBUG ("Base64解码完成，输出长度: " + to_string (decoded.size()));
	return decoded;
}

// 解析搜索结果页，提取影片列表
vector<MovieInfo> parseSearchResult (const string &html) {
	LOG_INFOR ("开始解析搜索结果页面，HTML总长度: " + to_string (html.size()));
	vector<MovieInfo> movies;

	if (html.empty()) {
		LOG_WARNG ("HTML内容为空，无法解析");
		return movies;
	}

	string key = "<ul class=\"stui-vodlist clearfix\">";
	size_t from = html.find (key);
	string container = html.substr (from + key.length(), html.substr (from + key.length()).find ("</ul>"));
	LOG_DEBUG ("video list find html:\033[38;2;255;127;0m" + container + "\033[0m");

	vector<string> list;

	while ((from = container.find ("<li")) != string::npos) {
		size_t to = container.find ("</li>");
		list.push_back (container.substr (from, to - from + 1));
		container.erase (from, to - from + 1);

		if (list.back().find ("电影") == string::npos) list.pop_back();
		else LOG_DEBUG ("get list #\033[38;2;255;127;0m" + list.back());
	}

	for (auto u : list) {
		u = u.substr (u.find ("<a href"));
		u = u.substr (0, u.find ("</a>"));
		LOG_DEBUG ("link #\033[38;2;255;127;0m" + u);
		movies.push_back ({"", "", ""});
		movies.back().detailUrl = "https://" + domain + u.substr (u.find ("href=") + 6, u.find ("\" target") - (u.find ("href=") + 6));
//		LOG_DEBUG("detail...");
		movies.back().playUrl = "https://" + domain + "/vodplay/" + u.substr (u.find ("/voddetail/") + 11, u.find (".html") - u.find ("/voddetail/") - 11) + "-1-1.html";
//		LOG_DEBUG("play...");
		movies.back().title = u.substr (u.find_last_of ('>') + 1);
//		LOG_DEBUG("title...");
		LOG_DEBUG ("struct #" + to_string (movies.size()) + ":\033[38;2;255;127;0m" + movies.back().detailUrl + "\033[0m;\033[38;2;255;127;0m" + movies.back().playUrl + "\033[0m;\033[38;2;255;127;0m" + movies.back().title);
	}

	LOG_INFOR ("解析完成，共找到" + to_string (movies.size()) + "部匹配的影片");
	return movies;
}
//vector<MovieInfo> parseSearchResult(const string& html) {
//	LOG_INFOR("解析搜索结果页面，HTML总长度: " + to_string(html.size()));
//	vector<MovieInfo> movies;
//
//	if (html.empty()) {
//		LOG_WARNG("HTML内容为空，无法解析");
//		return movies;
//	}
//
//	try {
//		regex reg("<li[^>]*class=\"col-md-6 col-sm-4 col-xs-3\"[^>]*>[\\s\\S]*?<a[^>]*href=\"(/voddetail/\\d+\\.html)\"[^>]*title=\"([^\"]+)\">[\\s\\S]*?</li>", regex::icase);
//		sregex_iterator it(html.begin(), html.end(), reg);
//		sregex_iterator end;
//
//		for (; it != end; ++it) {
//			smatch match = *it;
//			MovieInfo movie;
//			movie.title = match[2].str();
//			movie.detailUrl = "https://" + domain + match[1].str();
//
//			LOG_DEBUG("匹配到影片: 标题=[" + movie.title + "], 详情页=[" + movie.detailUrl + "]");
//
//			// 提取详情页ID，生成播放页链接
//			string detailPath = match[1].str();
//			regex idReg("\\d+");
//			smatch idMatch;
//			if (regex_search(detailPath, idMatch, idReg)) {
//				movie.playUrl = "https://" + domain + "/vodplay/" + idMatch.str() + "-1-1.html";
//				LOG_DEBUG("生成播放页链接: [" + movie.playUrl + "]");
//			} else {
//				LOG_WARNG("无法从详情页链接提取影片ID: [" + detailPath + "]");
//				continue;
//			}
//
//			movies.push_back(movie);
//		}
//	} catch (const regex_error& e) {
//		LOG_ERROR("正则表达式解析错误: " + string(e.what()));
//		return movies;
//	}
//
//	LOG_INFOR("解析完成，共找到" + to_string(movies.size()) + "部匹配的影片");
//	return movies;
//}

// 从播放页提取m3u8链接
string getM3u8Url (const string &playUrl) {
	LOG_INFOR ("开始自动解析M3U8链接，播放页: [" + playUrl + "]");

	// 第一步：获取播放页原始HTML
	string playHtml = httpGetRaw (playUrl);

	if (playHtml.empty()) {
		LOG_ERROR ("获取播放页失败！URL: [" + playUrl + "]");
		return "";
	}

	// 第二步：优先用正则提取encrypt类型
	string encryptType = "2"; // 默认值
	regex encryptReg ("\"encrypt\"\\s*:\\s*(\\d+)", regex::icase);
	smatch encryptMatch;

	if (regex_search (playHtml, encryptMatch, encryptReg)) {
		encryptType = encryptMatch[1].str();
		LOG_INFOR ("正则提取到encrypt类型: [" + encryptType + "]");
	} else {
		LOG_WARNG ("正则未找到encrypt字段，强制使用默认值: [2]");
	}

	// 第三步：手动提取url字段
	string encryptedUrl = "";
	size_t playerStart = playHtml.find ("player_aaaa");

	if (playerStart != string::npos) {
		size_t urlKeyPos = playHtml.find ("\"url\":\"", playerStart);

		if (urlKeyPos != string::npos) {
			urlKeyPos += 7; // 跳过 "\"url\":\""
			size_t urlEndPos = playHtml.find ("\"", urlKeyPos);

			if (urlEndPos != string::npos && urlEndPos > urlKeyPos) {
				encryptedUrl = playHtml.substr (urlKeyPos, urlEndPos - urlKeyPos);
				LOG_INFOR ("手动提取到加密URL: [" + encryptedUrl + "]");
			}
		}
	}

	// 检查url是否提取成功
	if (encryptedUrl.empty()) {
		LOG_ERROR ("未找到player_aaaa.url字段！");
		saveStringToFile (playHtml, "lasterror_url.html");
		return "";
	}

	// 第四步：解密url字段（Base64 + unescape）
	string decryptedUrl = encryptedUrl;

	if (encryptType == "2") {
		decryptedUrl = base64Decode (decryptedUrl);
		LOG_INFOR ("Base64解码后: [" + decryptedUrl + "]");

		decryptedUrl = unescape (decryptedUrl);
		LOG_INFOR ("unescape解密后（最终下载URL）: [" + decryptedUrl + "]");
	} else if (encryptType == "1") {
		decryptedUrl = unescape (decryptedUrl);
		LOG_INFOR ("unescape解密后（最终下载URL）: [" + decryptedUrl + "]");
	}

	return decryptedUrl;
}

// 搜索按钮点击处理函数
void OnSearchClicked() {
	// 禁用按钮防止重复点击
	EnableWindow (hBtnSearch, FALSE);
	EnableWindow (hBtnDownload, FALSE);

	// 清空影片列表
	SendMessageA (hListMovies, LB_RESETCONTENT, 0, 0);
	g_movieList.clear();

	// 获取用户输入的影片名
	char movieNameBuf[512] = {0};
	GetWindowTextA (hEditMovieName, movieNameBuf, sizeof (movieNameBuf));
	string movieName = movieNameBuf;
	EatTail (movieName);

	if (movieName.empty()) {
		MessageBoxA (hWndMain, "请输入要搜索的影片名称！", "提示", MB_ICONINFORMATION);
		EnableWindow (hBtnSearch, TRUE);
		return;
	}

	LOG_INFOR ("开始搜索影片: [" + movieName + "]");

	// 构建搜索URL
	string searchUrl = "https://" + domain + "/vodsearch/-------------.html?wd=" + urlEncode (movieName) + "&submit=";
	LOG_INFOR ("搜索请求URL: [" + searchUrl + "]");

	// 获取搜索结果
	string html = httpGet (searchUrl);

	if (html.empty()) {
		LOG_ERROR ("获取搜索结果失败！");
		MessageBoxA (hWndMain, "获取搜索结果失败，请检查网络连接！", "错误", MB_ICONERROR);
		EnableWindow (hBtnSearch, TRUE);
		return;
	}

	// 解析影片列表
	g_movieList = parseSearchResult (html);

	if (g_movieList.empty()) {
		LOG_WARNG ("未找到相关影片");
		MessageBoxA (hWndMain, ("未找到与\"" + movieName + "\"相关的影片！").c_str(), "提示", MB_ICONINFORMATION);
		EnableWindow (hBtnSearch, TRUE);
		return;
	}

	// 将影片添加到列表框
	for (size_t i = 0; i < g_movieList.size(); ++i) {
		int idx = SendMessageA (hListMovies, LB_ADDSTRING, 0, (LPARAM)g_movieList[i].title.c_str());
		SendMessageA (hListMovies, LB_SETITEMDATA, idx, (LPARAM)i);
	}

	LOG_INFOR ("搜索完成，共找到" + to_string (g_movieList.size()) + "部影片");
	MessageBoxA (hWndMain, ("搜索完成，共找到" + to_string (g_movieList.size()) + "部影片！").c_str(), "提示", MB_ICONINFORMATION);

	// 启用下载按钮
	EnableWindow (hBtnSearch, TRUE);
	EnableWindow (hBtnDownload, TRUE);
	EnableWindow (hBtnDetail, TRUE);
}

// 下载按钮点击处理函数
DWORD WINAPI DownloadThread (LPVOID lpParam) {
	// 获取选中的影片
	int selIdx = SendMessageA (hListMovies, LB_GETCURSEL, 0, 0);

	if (selIdx == LB_ERR) {
		MessageBoxA (hWndMain, "请先选择要下载的影片！", "提示", MB_ICONINFORMATION);
		EnableWindow (hBtnDownload, TRUE);
		EnableWindow (hBtnSearch, TRUE);
		return 0;
	}

	int movieIdx = (int)SendMessageA (hListMovies, LB_GETITEMDATA, selIdx, 0);
	MovieInfo selectedMovie = g_movieList[movieIdx];

	// 获取项目名称
	char projectBuf[512] = {0};
	GetWindowTextA (hEditProject, projectBuf, sizeof (projectBuf));
	g_projectName = projectBuf;
	EatTail (g_projectName);

	if (g_projectName.empty()) {
		MessageBoxA (hWndMain, "请输入项目名称！", "提示", MB_ICONINFORMATION);
		EnableWindow (hBtnDownload, TRUE);
		EnableWindow (hBtnSearch, TRUE);
		return 0;
	}

	// 生成随机十六进制临时文件夹名
	g_tempFolder = generateRandomHexString();
	LOG_INFOR ("生成随机临时文件夹名: [" + g_tempFolder + "]");

	try {
		// 更新进度条
		SendMessageA (hProgressBar, PBM_SETPOS, 10, 0);

		// 获取m3u8链接
		LOG_INFOR ("开始解析M3U8链接...");
		g_selectedM3u8Url = getM3u8Url (selectedMovie.playUrl);

		if (g_selectedM3u8Url.empty()) {
			LOG_ERROR ("M3U8链接解析失败！");
			MessageBoxA (hWndMain, "M3U8链接解析失败！", "错误", MB_ICONERROR);
			EnableWindow (hBtnDownload, TRUE);
			EnableWindow (hBtnSearch, TRUE);
			return 0;
		}

		LOG_INFOR ("成功获取M3U8链接: [" + g_selectedM3u8Url + "]");

		SendMessageA (hProgressBar, PBM_SETPOS, 20, 0);

		// 清理旧文件夹
		string rmOldDir = "rmdir /s /q \"" + g_tempFolder + "\" 2>nul";
		LOG_DEBUG ("执行旧目录清理命令: [" + rmOldDir + "]");
		system (rmOldDir.c_str());

		string mkdirCmd = "md \"" + g_tempFolder + "\"";
		LOG_DEBUG ("创建临时目录命令: [" + mkdirCmd + "]");
		system (mkdirCmd.c_str());

		SendMessageA (hProgressBar, PBM_SETPOS, 30, 0);

		// 下载M3U8文件
		LOG_INFOR ("开始下载M3U8文件...");
		string m3u8LocalFile = downloadSingle (g_selectedM3u8Url);
		LOG_INFOR ("M3U8文件已保存到: [" + m3u8LocalFile + "]");

		SendMessageA (hProgressBar, PBM_SETPOS, 40, 0);

		// 解析mixed
		LOG_INFOR ("开始解析M3U8文件...");
		string mixedUrl = downloadMixed (m3u8LocalFile, g_selectedM3u8Url);

		if (mixedUrl.empty()) {
			LOG_FATAL (string ("mixed下载失败！"));
			throw runtime_error ("mixed download failed");
		}

		LOG_INFOR ("mixed.M3U8解析完成");

		SendMessageA (hProgressBar, PBM_SETPOS, 50, 0);

		// 下载TS分片
		LOG_INFOR ("开始下载TS分片...");
		SendMessageA (hProgressBar, PBM_SETPOS, 60, 0); // 从50→60

		if (downloadM3u8 (m3u8LocalFile, mixedUrl) != 0) {
			LOG_FATAL (string ("TS分片下载失败！"));
			throw runtime_error ("TS segments download failed");
		}

		SendMessageA (hProgressBar, PBM_SETPOS, 80, 0); // TS下载完成→80

		// 拼接视频
		LOG_INFOR ("开始拼接视频...");

		if (vidcat ("ffmpeg.exe") != 0) {
			LOG_FATAL (string ("视频拼接失败！"));
			throw runtime_error ("Video concat failed");
		}

		SendMessageA (hProgressBar, PBM_SETPOS, 90, 0);

		// 清理临时文件
		string rmDirCmd = "rmdir /s /q \"" + g_tempFolder + "\"";
		LOG_DEBUG ("执行临时目录删除命令: [" + rmDirCmd + "]");
		system (rmDirCmd.c_str());
		LOG_INFOR ("临时文件已清理完成");

		SendMessageA (hProgressBar, PBM_SETPOS, 100, 0);

		// 完成
		LOG_INFOR ("下载完成！输出文件: " + g_projectName + ".mp4");
		MessageBoxA (hWndMain, ("下载完成！输出文件：" + g_projectName + ".mp4").c_str(), "完成", MB_ICONINFORMATION);

	} catch (const exception& e) {
		string errorMsg = "下载过程中发生异常: " + string (e.what());
		LOG_FATAL (errorMsg);
		MessageBoxA (hWndMain, errorMsg.c_str(), "错误", MB_ICONERROR);
	}

	// 重置进度条
	SendMessageA (hProgressBar, PBM_SETPOS, 0, 0);

	// 启用按钮
	EnableWindow (hBtnDownload, TRUE);
	EnableWindow (hBtnSearch, TRUE);

	return 0;
}
void OnDownloadClicked() {
	// 禁用按钮
	EnableWindow (hBtnDownload, FALSE);
	EnableWindow (hBtnSearch, FALSE);

	// 创建下载线程
	CreateThread (NULL, 0, DownloadThread, NULL, 0, NULL);
}

namespace DetailParse {
	// 移除字符串中的所有HTML标签
	string remove_html_tags (string str) {
		size_t start = 0, end = 0;

		while ((start = str.find ('<', start)) != string::npos) {
			end = str.find ('>', start);

			if (end == string::npos) break;

			str.erase (start, end - start + 1);
		}

		return str;
	}

// 替换字符串中的指定子串
	void replace_all (string& str, const string& from, const string& to) {
		size_t pos = 0;

		while ((pos = str.find (from, pos)) != string::npos) {
			str.replace (pos, from.length(), to);
			pos += to.length();
		}
	}

// 清理特殊字符和多余空格
	string clean_string (string str) {
		// 替换HTML特殊字符
		replace_all (str, "&nbsp;", " ");
		replace_all (str, "&amp;", "&");
		replace_all (str, "&#039;", "'");
		replace_all (str, ";#039;", "'");

		// 移除多余空格
		str.erase (unique (str.begin(), str.end(), [] (char a, char b) {
			return isspace (a) && isspace (b);
		}), str.end());

		// 移除首尾空格
		size_t first = str.find_first_not_of (" \t\n\r");
		size_t last = str.find_last_not_of (" \t\n\r");

		if (first == string::npos) return "";

		return str.substr (first, last - first + 1);
	}

// 提取指定特征字符串后的内容（到结束标记为止）
	string extract_content (const string& html, const string& start_mark,
	                        const string& end_mark, size_t &last_pos) {
		size_t start = html.find (start_mark, last_pos);

		if (start == string::npos) return "";

		start += start_mark.length();
		size_t end = html.find (end_mark, start);

		if (end == string::npos) end = html.length();

		string content = html.substr (start, end - start);
		last_pos = end; // 更新最后查找位置，提高效率
		return clean_string (remove_html_tags (content));
	}

// 解析
	vector<string> parse_movie_html (const string& html) {
		vector<string> result (8, ""); // 0:影片名 1:别名 2:类型 3:主演 4:导演 5:地区 6:更新 7:简介
		size_t last_pos = 0;

		// 1. 定位核心容器，缩小解析范围
		string container_start = "<div class=\"stui-content__detail\">";
		string container_end = "</div>";
		size_t container_begin = html.find (container_start);

		if (container_begin == string::npos) return result;

		container_begin += container_start.length();
		size_t container_finish = html.find (container_end, container_begin);

		if (container_finish == string::npos) container_finish = html.length();

		string core_html = html.substr (container_begin, container_finish - container_begin);
		last_pos = 0; // 重置查找位置，在核心容器内查找

		// 2. 提取影片名 (class="title" 到 </h1>)
		result[0] = extract_content (core_html, "class=\"title\">", "</h1>", last_pos);

		// 3. 提取别名 (别名： 到 </p>)
		result[1] = extract_content (core_html, "别名：", "</p>", last_pos);

		// 4. 提取类型 (类型： 到 </p>)
		result[2] = extract_content (core_html, "类型：", "</p>", last_pos);

		// 5. 提取主演 (主演： 到 </p>)
		result[3] = extract_content (core_html, "主演：", "</p>", last_pos);

		// 6. 提取导演 (导演： 到 </p>)
		result[4] = extract_content (core_html, "导演：", "</p>", last_pos);

		// 7. 提取地区 (地区： 到 </p>)
		result[5] = extract_content (core_html, "地区：", "</p>", last_pos);

		// 8. 提取更新时间 (更新： 到 </p>)
		result[6] = extract_content (core_html, "更新：", "</p>", last_pos);

		// 9. 提取简介（合并detail-sketch和detail-content）
		string desc_sketch = extract_content (core_html, "detail-sketch\">", "</span>", last_pos);
		string desc_content = extract_content (core_html, "detail-content\" style=\"display: none;\">", "</span>", last_pos);
		result[7] = clean_string (desc_sketch + desc_content);

		return result;
	}
}

void OnDetailClicked() {
	// 禁用按钮
	EnableWindow (hBtnDetail, FALSE);
	// 获取选中的影片
	int selIdx = SendMessageA (hListMovies, LB_GETCURSEL, 0, 0);

	if (selIdx == LB_ERR) {
		MessageBoxA (hWndMain, "请先选择要查看详细信息的影片！", "提示", MB_ICONINFORMATION);
		EnableWindow (hBtnDetail, TRUE);
		return;
	}

	int movieIdx = (int)SendMessageA (hListMovies, LB_GETITEMDATA, selIdx, 0);
	MovieInfo selectedMovie = g_movieList[movieIdx];

	string sDetail = selectedMovie.detailUrl;
	string response = httpGet (sDetail);

	LOG_DEBUG ("http get detail page #\033[38;2;255;127;0m" + response);

	string keyDetail = "<div class=\"stui-content__detail\">";
	string sDetailHtml = response.substr (response.find (keyDetail));
	sDetailHtml.erase (sDetailHtml.find ("</div>") + 6);
	LOG_DEBUG ("get movie detail #\033[38;2;255;127;0m" + sDetailHtml);

	vector<string> detail = DetailParse::parse_movie_html (sDetailHtml);
	string det = "";
	det += "        片名：" + detail[0] + "\n";
	det += "        别名：" + detail[1] + "\n";
	det += "        类型：" + detail[2] + "\n";
	det += "        主演：" + detail[3] + "\n";
	det += "        导演：" + detail[4] + "\n";
	det += "        地区：" + detail[5] + "\n";
	det += "        更新：" + detail[6] + "\n";
	det += "        简介：" + detail[7];

	MessageBoxA (NULL, det.data(), (detail[0] + " 信息").data(), MB_ICONINFORMATION);

	// 启用按钮
	EnableWindow (hBtnDetail, TRUE);
}

void OnSyncClicked() {
	// 获取用户输入的影片名
	char movieNameBuf[512] = {0};
	GetWindowTextA (hEditMovieName, movieNameBuf, sizeof (movieNameBuf));
	string movieName = movieNameBuf;
	EatTail (movieName);

	SetWindowTextA (hEditProject, movieName.data());
}

void OnCurlChkClicked() {
	UINT ChkState = (UINT)SendMessageA (
	                    hChkCurl,
	                    BM_GETCHECK,
	                    0,
	                    0
	                );

	switch (ChkState) {
		case BST_CHECKED:
			g_useCurl = true;
			LOG_INFOR ("将使用curl下载");
			break;

		case BST_UNCHECKED:
			g_useCurl = false;
			LOG_INFOR ("将使用aria2c下载");
			break;

		default:
			g_useCurl = false;
			LOG_INFOR ("将使用aria2c下载");
	}
}

// 窗口过程函数
LRESULT CALLBACK WndProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
				// 创建控件
				// 项目名称标签
				HWND hProjH = CreateWindowA (
				                  "STATIC",
				                  "项目名称:",
				                  WS_VISIBLE | WS_CHILD | SS_LEFT,
				                  10, 10, 80, 20,
				                  hWnd,
				                  NULL,
				                  GetModuleHandle (NULL),
				                  NULL
				              );

				// 项目名称输入框
				hEditProject 	= CreateWindowA ("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
				                                90, 10, 300, 25, hWnd, NULL, GetModuleHandle (NULL), NULL);
				// 同步按钮
				hBtnSync 		= CreateWindowA ("BUTTON", "同步名称", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
				                             400, 10, 80, 25, hWnd, (HMENU)4, GetModuleHandle (NULL), NULL);

				// 影片名称标签
				HWND hFilmH 	= CreateWindowA ("STATIC", "影片名称:", WS_VISIBLE | WS_CHILD | SS_LEFT,
				                               10, 45, 80, 20, hWnd, NULL, GetModuleHandle (NULL), NULL);

				// 影片名称输入框
				hEditMovieName 	= CreateWindowA ("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
				                                 90, 45, 300, 25, hWnd, NULL, GetModuleHandle (NULL), NULL);

				// 搜索按钮
				hBtnSearch 		= CreateWindowA ("BUTTON", "搜索影片", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
				                               400, 45, 80, 25, hWnd, (HMENU)1, GetModuleHandle (NULL), NULL);

				// 影片列表框
				hListMovies 	= CreateWindowA ("LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
				                               10, 80, 470, 150, hWnd, NULL, GetModuleHandle (NULL), NULL);

				// 下载按钮
				hBtnDownload 	= CreateWindowA ("BUTTON", "开始下载", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
				                                400, 240, 80, 25, hWnd, (HMENU)2, GetModuleHandle (NULL), NULL);
				// 详情按钮
				hBtnDetail 		= CreateWindowA ("BUTTON", "影片详情", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
				                               300, 240, 80, 25, hWnd, (HMENU)3, GetModuleHandle (NULL), NULL);
				// curl下载复选框
				hChkCurl 		= CreateWindowA ("BUTTON", "使用curl下载", WS_VISIBLE | WS_CHILD | WS_BORDER | BS_AUTOCHECKBOX,
				                             160, 240, 120, 25, hWnd, (HMENU)5, GetModuleHandle (NULL), NULL);

				// 进度条（总进度）
				hProgressBar 	= CreateWindowA ("msctls_progress32", "", WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
				                                10, 275, 470, 25, hWnd, NULL, GetModuleHandle (NULL), NULL);

				// TS分片进度条
				HWND hProgH 	= CreateWindowA ("STATIC", "分片下载进度:", WS_VISIBLE | WS_CHILD | SS_LEFT,
				                               10, 305, 120, 20, hWnd, NULL, GetModuleHandle (NULL), NULL);
				hProgressBarTS 	= CreateWindowA ("msctls_progress32", "", WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
				                                 110, 305, 370, 25, hWnd, NULL, GetModuleHandle (NULL), NULL);

				// 日志编辑框（启用彩色文本）
				HWND hLogH 		= CreateWindowA ("STATIC", "日志信息:", WS_VISIBLE | WS_CHILD | SS_LEFT,
				                               10, 335, 80, 20, hWnd, NULL, GetModuleHandle (NULL), NULL);
				hEditLog 		= CreateWindowA ("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE |
				                             ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL,
				                             10, 360, 470, 180, hWnd, NULL, GetModuleHandle (NULL), NULL);

				// 设置字体
				HFONT hFontChn 	= CreateFontA (16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "仿宋");
				HFONT hFontEng 	= CreateFontA (20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Cascadia Code");
				SendMessageA (hEditProject, 	WM_SETFONT, 	(WPARAM)hFontEng, TRUE);
				SendMessageA (hEditMovieName, 	WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hListMovies, 		WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hEditLog, 		WM_SETFONT, 	(WPARAM)hFontEng, TRUE);
				SendMessageA (hBtnSearch, 		WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hBtnDownload, 	WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hBtnDetail, 		WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hBtnSync, 		WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hChkCurl, 		WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hProjH, 			WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hFilmH, 			WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hProgH, 			WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hLogH, 			WM_SETFONT, 	(WPARAM)hFontChn, TRUE);
				SendMessageA (hProgressBar, 	PBM_SETRANGE, 	0, 				  MAKELPARAM (0, 100));
				SendMessageA (hProgressBar, 	PBM_SETPOS, 	0, 				  0);
				SendMessageA (hProgressBarTS, 	PBM_SETRANGE, 	0, 				  MAKELPARAM (0, Max));
				SendMessageA (hProgressBarTS, 	PBM_SETPOS, 	0, 				  0);

				SetWindowTextA (hEditProject, "默认项目");

				LOG_INFOR(license);
		
				mt19937 mt (time (NULL));

				domain = domains[mt() % nDomain];
				LOG_INFOR ("使用域 " + domain);

				break;
			}

		case WM_COMMAND: {
				switch (LOWORD (wParam)) {
					case 1: // 搜索按钮
						OnSearchClicked();
						break;

					case 2: // 下载按钮
						OnDownloadClicked();
						break;

					case 3: // 详情按钮
						OnDetailClicked();
						break;

					case 4: // 同步按钮
						OnSyncClicked();
						break;

					case 5: // curl复选框
						OnCurlChkClicked();
						break;
				}

				break;
			}

		case WM_DESTROY:
			PostQuitMessage (0);
			break;

		default:
			return DefWindowProcA (hWnd, msg, wParam, lParam);
	}

	return 0;
}

// 管道读取线程句柄
//HANDLE hPipeThreadOut = NULL;
//HANDLE hPipeThreadErr = NULL;

// 管道读取线程函数（处理STDOUT）
//DWORD WINAPI ReadPipeThread(LPVOID lpParam) {
//    HANDLE hReadPipe = (HANDLE)lpParam;
//    char buffer[4096];
//    DWORD bytesRead;
//
//    while (ReadFile(hReadPipe, buffer, sizeof(buffer)-1, &bytesRead, NULL) && bytesRead > 0) {
//        buffer[bytesRead] = 0;
//        string str(buffer);
//        // 替换换行符适配编辑框，清理多余回车
//        replace(str, '\n', '\r');
//        replace(str, '\r\r', '\r');
//        if (!str.empty()) {
//            LogToEdit("STDOUT", str, 0x808080); // 灰色显示控制台输出
//        }
//    }
//    return 0;
//}
//
//// 管道读取线程函数（处理STDERR）
//DWORD WINAPI ReadPipeThreadErr(LPVOID lpParam) {
//    HANDLE hReadPipe = (HANDLE)lpParam;
//    char buffer[4096];
//    DWORD bytesRead;
//
//    while (ReadFile(hReadPipe, buffer, sizeof(buffer)-1, &bytesRead, NULL) && bytesRead > 0) {
//        buffer[bytesRead] = 0;
//        string str(buffer);
//        replace(str, '\n', '\r');
//        replace(str, '\r\r', '\r');
//        if (!str.empty()) {
//            LogToEdit("STDERR", str, 0xFF0000); // 红色显示错误输出
//        }
//    }
//    return 0;
//}

// 重定向控制台输出
//void RedirectConsoleOutput() {
//    // 1. 隐藏控制台窗口（如果存在）
//    HWND hConsole = GetConsoleWindow();
//    if (hConsole != NULL) {
//        ShowWindow(hConsole, SW_HIDE);
//    }
//
//    // 2. 创建匿名管道用于重定向STDOUT
//    HANDLE hReadPipeOut, hWritePipeOut;
//    SECURITY_ATTRIBUTES sa = {0};
//    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
//    sa.bInheritHandle = TRUE;
//    sa.lpSecurityDescriptor = NULL;
//
//    if (!CreatePipe(&hReadPipeOut, &hWritePipeOut, &sa, 0)) {
//        LogToEdit("ERROR", "创建STDOUT管道失败: " + to_string(GetLastError()), 0xFF0000);
//        return;
//    }
//
//    // 3. 创建匿名管道用于重定向STDERR
//    HANDLE hReadPipeErr, hWritePipeErr;
//    if (!CreatePipe(&hReadPipeErr, &hWritePipeErr, &sa, 0)) {
//        LogToEdit("ERROR", "创建STDERR管道失败: " + to_string(GetLastError()), 0xFF0000);
//        CloseHandle(hReadPipeOut);
//        CloseHandle(hWritePipeOut);
//        return;
//    }
//
//    // 4. 重定向标准输出和错误
//    SetStdHandle(STD_OUTPUT_HANDLE, hWritePipeOut);
//    SetStdHandle(STD_ERROR_HANDLE, hWritePipeErr);
//
//    // 5. 关闭不需要的写端（子进程继承后，父进程关闭）
//    CloseHandle(hWritePipeOut);
//    CloseHandle(hWritePipeErr);
//
//    // 6. 创建线程读取管道内容
//    hPipeThreadOut = CreateThread(NULL, 0, ReadPipeThread, hReadPipeOut, 0, NULL);
//    hPipeThreadErr = CreateThread(NULL, 0, ReadPipeThreadErr, hReadPipeErr, 0, NULL);
//
//    // 7. 同时重定向C++流
//    std::cout.rdbuf(&g_coutBuffer);
//    std::cerr.rdbuf(&g_cerrBuffer);
//}

// 主函数
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// 初始化通用控件
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof (INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx (&icex);

	LoadLibraryA ("riched20.dll");

	// 重定向控制台输出到日志框
//    RedirectConsoleOutput();

	// 解析命令行参数
	if (string (lpCmdLine).find ("--debug") != string::npos) {
		g_debugMode = true;
		printf ("Debug mode opened.\nCommand Line = %s\n", lpCmdLine);
	} else FreeConsole();

	// 注册窗口类
	WNDCLASSEXA wc;
	ZeroMemory (&wc, sizeof (WNDCLASSEXA));
	wc.cbSize        = sizeof (WNDCLASSEXA);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = hInstance;
	wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = "DownloaderWindowClass";

	if (!RegisterClassExA (&wc)) {
		MessageBoxA (NULL, "窗口类注册失败！", "错误", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// 创建主窗口
	hWndMain = CreateWindowExA (
	               WS_EX_CLIENTEDGE,
	               "DownloaderWindowClass",
	               "AutomaticMovieDownloaderV3.2 by hdkghc (ZnBkCu)",
	               WS_OVERLAPPEDWINDOW,
	               CW_USEDEFAULT, CW_USEDEFAULT, 510, 580,
	               NULL, NULL, hInstance, NULL
	           );

	if (hWndMain == NULL) {
		MessageBoxA (NULL, "窗口创建失败！", "错误", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// 显示窗口
	ShowWindow (hWndMain, nCmdShow);
	UpdateWindow (hWndMain);

	// 初始化符号解析
	SymInitialize (GetCurrentProcess(), NULL, TRUE);

	// 消息循环
	MSG msg;

	while (GetMessageA (&msg, NULL, 0, 0) > 0) {
		TranslateMessage (&msg);
		DispatchMessageA (&msg);
	}

	// 清理符号信息
	SymCleanup (GetCurrentProcess());

	return msg.wParam;
}
