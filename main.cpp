#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <iostream>
#include <mysql.h>
#include <string>
#include <memory>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "libmysql.lib")

LPDIRECT3D9 g_pD3D = nullptr;
LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
D3DPRESENT_PARAMETERS g_d3dpp = {};
std::unique_ptr<MYSQL> g_mysql;

bool InitD3D(HWND hWnd)
{
    // init your mysql connection here
    g_mysql = std::unique_ptr<MYSQL>(mysql_init(nullptr));
    if (!g_mysql || !mysql_real_connect(g_mysql.get(), "localhost", "root", "password", 
                                      "cheat_db", 3306, nullptr, 0)) {
        return false;
    }

    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &g_d3dpp, &g_pd3dDevice)))
    {
        mysql_close(g_mysql.get());
        return false;
    }

    return true;
}
