#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <functional>
#include "../Core/Value.h"

// Dynamic WinHttp loader
class HttpClient {
    struct API {
        HMODULE hMod = nullptr;
        decltype(&WinHttpCrackUrl) CrackUrl = nullptr;
        decltype(&WinHttpOpen) Open = nullptr;
        decltype(&WinHttpConnect) Connect = nullptr;
        decltype(&WinHttpOpenRequest) OpenRequest = nullptr;
        decltype(&WinHttpAddRequestHeaders) AddRequestHeaders = nullptr;
        decltype(&WinHttpSendRequest) SendRequest = nullptr;
        decltype(&WinHttpReceiveResponse) ReceiveResponse = nullptr;
        decltype(&WinHttpQueryHeaders) QueryHeaders = nullptr;
        decltype(&WinHttpQueryDataAvailable) QueryDataAvailable = nullptr;
        decltype(&WinHttpReadData) ReadData = nullptr;
        decltype(&WinHttpCloseHandle) CloseHandle = nullptr;

        bool init() {
            if (hMod) return true;
            hMod = LoadLibraryA("winhttp.dll");
            if (!hMod) return false;
            CrackUrl = (decltype(CrackUrl))GetProcAddress(hMod, "WinHttpCrackUrl");
            Open = (decltype(Open))GetProcAddress(hMod, "WinHttpOpen");
            Connect = (decltype(Connect))GetProcAddress(hMod, "WinHttpConnect");
            OpenRequest = (decltype(OpenRequest))GetProcAddress(hMod, "WinHttpOpenRequest");
            AddRequestHeaders = (decltype(AddRequestHeaders))GetProcAddress(hMod, "WinHttpAddRequestHeaders");
            SendRequest = (decltype(SendRequest))GetProcAddress(hMod, "WinHttpSendRequest");
            ReceiveResponse = (decltype(ReceiveResponse))GetProcAddress(hMod, "WinHttpReceiveResponse");
            QueryHeaders = (decltype(QueryHeaders))GetProcAddress(hMod, "WinHttpQueryHeaders");
            QueryDataAvailable = (decltype(QueryDataAvailable))GetProcAddress(hMod, "WinHttpQueryDataAvailable");
            ReadData = (decltype(ReadData))GetProcAddress(hMod, "WinHttpReadData");
            CloseHandle = (decltype(CloseHandle))GetProcAddress(hMod, "WinHttpCloseHandle");
            return Open != nullptr;
        }
    };
    static inline API api;

public:
    struct Response {
        int status = 0;
        std::string body;
        std::vector<std::pair<std::string, std::string>> headers;
        bool ok = false;
        std::string error;
    };

    // Synchronous fetch
    static Response Fetch(const std::string& url, const std::string& method = "GET",
                          const std::string& bodyData = "",
                          const std::vector<std::pair<std::string, std::string>>& headers = {}) {
        Response resp;

        // Parse URL
        std::wstring wUrl(url.begin(), url.end());
        URL_COMPONENTS uc = {};
        uc.dwStructSize = sizeof(uc);
        wchar_t scheme[16], host[256], path[2048];
        uc.lpszScheme = scheme; uc.dwSchemeLength = 16;
        uc.lpszHostName = host; uc.dwHostNameLength = 256;
        uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
        
        if (!api.init()) {
            resp.error = "WinHttp library could not be loaded";
            return resp;
        }

        if (!api.CrackUrl(wUrl.c_str(), (DWORD)wUrl.size(), 0, &uc)) {
            resp.error = "Invalid URL";
            return resp;
        }

        HINTERNET hSession = api.Open(L"DOM/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) { resp.error = "Session failed"; return resp; }

        HINTERNET hConnect = api.Connect(hSession, host, uc.nPort, 0);
        if (!hConnect) { api.CloseHandle(hSession); resp.error = "Connect failed"; return resp; }

        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        std::wstring wMethod(method.begin(), method.end());
        HINTERNET hRequest = api.OpenRequest(hConnect, wMethod.c_str(), path,
                                                 NULL, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            api.CloseHandle(hConnect);
            api.CloseHandle(hSession);
            resp.error = "Request failed";
            return resp;
        }

        // Add headers
        for (auto& h : headers) {
            std::wstring header(h.first.begin(), h.first.end());
            header += L": ";
            std::wstring val(h.second.begin(), h.second.end());
            header += val;
            api.AddRequestHeaders(hRequest, header.c_str(), (DWORD)header.size(), WINHTTP_ADDREQ_FLAG_ADD);
        }

        // Send
        BOOL sent = api.SendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        bodyData.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)bodyData.c_str(),
                                        (DWORD)bodyData.size(), (DWORD)bodyData.size(), 0);
        if (!sent || !api.ReceiveResponse(hRequest, NULL)) {
            api.CloseHandle(hRequest);
            api.CloseHandle(hConnect);
            api.CloseHandle(hSession);
            resp.error = "Send/Receive failed";
            return resp;
        }

        // Read status
        DWORD status = 0, statusSize = sizeof(status);
        api.QueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            NULL, &status, &statusSize, NULL);
        resp.status = (int)status;
        resp.ok = (status >= 200 && status < 300);

        // Read body
        DWORD bytesAvailable = 0;
        while (api.QueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            api.ReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            resp.body.append(buf.data(), bytesRead);
        }

        api.CloseHandle(hRequest);
        api.CloseHandle(hConnect);
        api.CloseHandle(hSession);
        return resp;
    }

    // Async fetch that posts result back to window message queue
    struct AsyncState {
        std::string url, method, body;
        std::vector<std::pair<std::string, std::string>> headers;
        std::function<void(Response)> callback;
    };

    static void FetchAsync(const std::string& url, const std::string& method,
                           const std::string& body,
                           const std::vector<std::pair<std::string, std::string>>& headers,
                           std::function<void(Response)> callback) {
        AsyncState* state = new AsyncState{url, method, body, headers, callback};
        CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
            AsyncState* s = (AsyncState*)param;
            auto resp = Fetch(s->url, s->method, s->body, s->headers);
            s->callback(resp);
            delete s;
            return 0;
        }, state, 0, NULL);
    }
};
