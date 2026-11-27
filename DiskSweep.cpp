// ============================================================================
//
//    ____  _     _      ____
//   |  _ \(_)___| | __ / ___|_      _____  ___ _ __
//   | | | | / __| |/ / \___ \ \ /\ / / _ \/ _ \ '_ \
//   | |_| | \__ \   <   ___) \ V  V /  __/  __/ |_) |
//   |____/|_|___/_|\_\ |____/ \_/\_/ \___|\___| .__/
//                                             |_|
//
//  DiskSweep v2.0.0 - Liberador de espacio en disco para Windows
//  (antes conocido como GrifoCleaner v1.0)
//
//  SIN librerias externas: solo <windows.h> y la libreria estandar.
//  Funciona en C++11 o superior (no necesita flags especiales).
//
//  Compilar (el icono va incrustado desde DiskSweep.rc):
//     Lo facil    :  build.bat
//     MinGW / g++ :  windres DiskSweep.rc -O coff -o DiskSweep_res.o
//                    g++ DiskSweep.cpp DiskSweep_res.o -o DiskSweep.exe -static
//     MSVC        :  rc DiskSweep.rc
//                    cl /EHsc /O2 DiskSweep.cpp DiskSweep.res
//
//  Opciones:
//     -n, --dry-run   Simula (no borra nada, solo calcula)
//     -d, --deep      Limpieza profunda (logs de eventos, DISM, fuentes...)
//     -y, --yes       No pedir confirmacion
//         --no-pause  No esperar Enter al final
//     -v, --version   Muestra la version
//     -h, --help      Ayuda
// ============================================================================

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

typedef unsigned long long u64;

#define APP_NAME     "DiskSweep"
#define APP_VERSION  "2.0.0"
#define APP_TAGLINE  "Liberador de espacio en disco para Windows"

// ----------------------------------------------------------------------------
//  Colores ANSI (se desactivan solos si la consola no los soporta)
// ----------------------------------------------------------------------------
static const char *cReset = "", *cBold = "", *cRed = "", *cGreen = "",
                  *cYellow = "", *cCyan = "", *cBlue = "", *cMagenta = "",
                  *cGray = "";
static bool g_ansi = false;

static void enable_console() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode) &&
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        g_ansi = true;
        cReset = "\x1b[0m";   cBold = "\x1b[1m";
        cRed = "\x1b[91m";    cGreen = "\x1b[92m";  cYellow = "\x1b[93m";
        cCyan = "\x1b[96m";   cBlue = "\x1b[94m";   cMagenta = "\x1b[95m";
        cGray = "\x1b[90m";
    }
}

// Icono y titulo de la ventana (el icono 1 va incrustado en el .exe, ver DiskSweep.rc)
static void set_window_icon() {
    SetConsoleTitleA(APP_NAME " v" APP_VERSION);
    HWND w = GetConsoleWindow();
    if (!w) return;
    HINSTANCE inst = GetModuleHandleW(NULL);
    HANDLE ico_big = LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    HANDLE ico_min = LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!ico_big)
        ico_big = LoadImageW(NULL, L"logo.ico", IMAGE_ICON,
                             GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
    if (!ico_min)
        ico_min = LoadImageW(NULL, L"logo.ico", IMAGE_ICON,
                             GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
    if (ico_big) SendMessageW(w, WM_SETICON, ICON_BIG,   (LPARAM)ico_big);
    if (ico_min) SendMessageW(w, WM_SETICON, ICON_SMALL, (LPARAM)ico_min);
}

static void hide_cursor(bool hide) {
    if (g_ansi) std::cout << (hide ? "\x1b[?25l" : "\x1b[?25h");
}

static void clear_line() {
    if (g_ansi) std::cout << "\r\x1b[2K";
    else        std::cout << "\r" << std::string(78, ' ') << "\r";
}

// ----------------------------------------------------------------------------
//  Utilidades
// ----------------------------------------------------------------------------
struct Options { bool dry, deep, yes, pause; Options() : dry(false), deep(false), yes(false), pause(true) {} };

struct Ctx { bool dry; u64 bytes, files, errors; Ctx() : dry(false), bytes(0), files(0), errors(0) {} };

static std::string g_label;   // texto de la tarea actual (para el spinner)

static void spin() {
    static DWORD last = 0;
    static int f = 0;
    DWORD now = GetTickCount();
    if (now - last < 90) return;
    last = now;
    const char fr[] = "|/-\\";
    std::cout << "\r  " << cCyan << fr[f++ % 4] << cReset << " " << g_label
              << cGray << " ..." << cReset << std::flush;
}

static std::wstring env(const wchar_t* name) {
    wchar_t buf[4096];
    DWORD n = GetEnvironmentVariableW(name, buf, 4096);
    if (n == 0 || n >= 4096) return L"";
    return std::wstring(buf);
}

static std::wstring join(const std::wstring& a, const wchar_t* b) {
    if (a.empty()) return L"";
    return a + L"\\" + b;
}

static std::string human(u64 b) {
    const char* u[] = {"B", "KB", "MB", "GB", "TB"};
    double v = (double)b;
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    char out[48];
    std::snprintf(out, sizeof out, i == 0 ? "%.0f %s" : "%.1f %s", v, u[i]);
    return out;
}

static bool starts_ci(const std::wstring& s, const std::wstring& p) {
    if (s.size() < p.size()) return false;
    for (size_t i = 0; i < p.size(); ++i)
        if (std::towlower(s[i]) != std::towlower(p[i])) return false;
    return true;
}

static bool ends_ci(const std::wstring& s, const std::wstring& e) {
    if (s.size() < e.size()) return false;
    return starts_ci(s.substr(s.size() - e.size()), e);
}

// Ejecuta un comando oculto y espera a que termine
static int run_cmd(const std::wstring& cmd) {
    std::wstring line = L"cmd.exe /c " + cmd;
    std::vector<wchar_t> buf(line.begin(), line.end());
    buf.push_back(0);
    STARTUPINFOW si; ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof pi);
    if (!CreateProcessW(NULL, &buf[0], NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi))
        return -1;
    while (WaitForSingleObject(pi.hProcess, 100) == WAIT_TIMEOUT) spin();
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

// ---- Funciones de shell32 cargadas en tiempo de ejecucion (sin enlazar libs) --
#pragma pack(push, 1)
struct RBInfo { DWORD cbSize; LONGLONG size; LONGLONG items; };
#pragma pack(pop)

static bool is_admin() {
    typedef BOOL (WINAPI *Fn)();
    HMODULE m = LoadLibraryW(L"shell32.dll");
    if (!m) return false;
    Fn f = (Fn)GetProcAddress(m, "IsUserAnAdmin");
    return f ? f() != FALSE : false;
}

static void recycle_bin(Ctx& c) {
    typedef HRESULT (WINAPI *Q)(LPCWSTR, RBInfo*);
    typedef HRESULT (WINAPI *E)(HWND, LPCWSTR, DWORD);
    HMODULE m = LoadLibraryW(L"shell32.dll");
    if (!m) return;
    Q q = (Q)GetProcAddress(m, "SHQueryRecycleBinW");
    E e = (E)GetProcAddress(m, "SHEmptyRecycleBinW");
    if (q) {
        RBInfo info; ZeroMemory(&info, sizeof info); info.cbSize = sizeof info;
        if (q(NULL, &info) == S_OK) { c.bytes += (u64)info.size; c.files += (u64)info.items; }
    }
    if (!c.dry && e) e(NULL, NULL, 1 | 2 | 4);  // sin confirmar, sin barra, sin sonido
}

static void relaunch_as_admin(const Options& o) {
    typedef HINSTANCE (WINAPI *Fn)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
    HMODULE m = LoadLibraryW(L"shell32.dll");
    if (!m) return;
    Fn f = (Fn)GetProcAddress(m, "ShellExecuteW");
    if (!f) return;
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    std::wstring args = L"-y";
    if (o.dry)  args += L" -n";
    if (o.deep) args += L" -d";
    f(NULL, L"runas", exe, args.c_str(), NULL, SW_SHOWNORMAL);
}

// ----------------------------------------------------------------------------
//  Registro de Windows y espacio en disco
// ----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#endif
#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY 0x0100
#endif

// Borra TODOS los valores de una clave (ej: el historial de Win+R)
static void reg_clear_values(HKEY root, const wchar_t* sub, Ctx& c) {
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    std::vector<std::wstring> names;
    std::vector<wchar_t> name(16384);
    for (DWORD i = 0;; ++i) {
        DWORD len = (DWORD)name.size();
        if (RegEnumValueW(k, i, &name[0], &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        names.push_back(std::wstring(&name[0], len));
    }
    for (size_t i = 0; i < names.size(); ++i) {
        if (c.dry) { ++c.files; continue; }
        if (RegDeleteValueW(k, names[i].c_str()) == ERROR_SUCCESS) ++c.files; else ++c.errors;
    }
    RegCloseKey(k);
}

// Espacio libre del disco del sistema (para comparar antes/despues)
static u64 free_space() {
    std::wstring d = env(L"SystemDrive");
    if (d.empty()) d = L"C:";
    d += L"\\";
    ULARGE_INTEGER f, t, tf;
    if (GetDiskFreeSpaceExW(d.c_str(), &f, &t, &tf)) return f.QuadPart;
    return 0;
}

// Prepara y ejecuta el Liberador de espacio de Windows (cleanmgr) sin interaccion
static void run_cleanmgr(Ctx& c) {
    if (c.dry) return;
    const std::wstring path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches";
    HKEY root;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0,
                      KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &root) != ERROR_SUCCESS) return;
    std::vector<std::wstring> subs;
    wchar_t nm[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = 256;
        if (RegEnumKeyExW(root, i, nm, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        subs.push_back(nm);
    }
    RegCloseKey(root);

    for (size_t i = 0; i < subs.size(); ++i) {
        // Seguridad: NO tocar la carpeta Descargas ni instalaciones previas (Windows.old)
        if (subs[i] == L"DownloadsFolder" || subs[i] == L"Previous Installations") continue;
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (path + L"\\" + subs[i]).c_str(), 0,
                          KEY_SET_VALUE | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
            DWORD v = 2;   // 2 = marcar esta categoria para limpiar
            RegSetValueExW(k, L"StateFlags0099", 0, REG_DWORD, (const BYTE*)&v, sizeof v);
            RegCloseKey(k);
        }
    }
    u64 before = free_space();
    run_cmd(L"cleanmgr.exe /sagerun:99");
    u64 after = free_space();
    if (after > before) c.bytes += after - before;
}

// ----------------------------------------------------------------------------
//  Borrado seguro (ignora archivos en uso o sin permisos)
// ----------------------------------------------------------------------------
static void list_dir(const std::wstring& dir, std::vector<std::wstring>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
            out.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void wipe(const std::wstring& p, Ctx& c, bool remove_self) {
    if (p.empty()) return;
    DWORD a = GetFileAttributesW(p.c_str());
    if (a == INVALID_FILE_ATTRIBUTES) return;

    const bool is_dir  = (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const bool is_link = (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

    if (is_dir && !is_link) {
        std::vector<std::wstring> kids;
        list_dir(p, kids);
        for (size_t i = 0; i < kids.size(); ++i) wipe(p + L"\\" + kids[i], c, true);
        if (remove_self && !c.dry) RemoveDirectoryW(p.c_str());
        return;
    }

    u64 sz = 0;
    if (!is_dir) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d))
            sz = ((u64)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    }
    if (c.dry) { c.bytes += sz; ++c.files; return; }

    SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
    BOOL ok = is_dir ? RemoveDirectoryW(p.c_str()) : DeleteFileW(p.c_str());
    if (ok) { c.bytes += sz; ++c.files; } else ++c.errors;
    if (((c.files + c.errors) & 15) == 0) spin();
}

static void wipe_contents(const std::wstring& dir, Ctx& c) { wipe(dir, c, false); }

static void wipe_if(const std::wstring& dir, Ctx& c,
                    const std::function<bool(const std::wstring&)>& match) {
    if (dir.empty()) return;
    std::vector<std::wstring> kids;
    list_dir(dir, kids);
    for (size_t i = 0; i < kids.size(); ++i)
        if (match(kids[i])) wipe(dir + L"\\" + kids[i], c, true);
}

static void clean_chromium(const std::wstring& base, Ctx& c) {
    if (base.empty()) return;
    static const wchar_t* subs[] = {L"Cache", L"Code Cache", L"GPUCache",
                                    L"Service Worker\\CacheStorage", L"DawnCache",
                                    L"GrShaderCache"};
    std::vector<std::wstring> kids;
    list_dir(base, kids);
    for (size_t i = 0; i < kids.size(); ++i) {
        if (kids[i] == L"Default" || starts_ci(kids[i], L"Profile"))
            for (size_t s = 0; s < sizeof(subs) / sizeof(subs[0]); ++s)
                wipe_contents(base + L"\\" + kids[i] + L"\\" + subs[s], c);
    }
    wipe_contents(base + L"\\ShaderCache", c);
    wipe_contents(base + L"\\GrShaderCache", c);
}

// ----------------------------------------------------------------------------
//  Tareas
// ----------------------------------------------------------------------------
struct Task {
    std::string name;
    bool needs_admin;
    bool deep_only;
    std::function<void(Ctx&)> run;
};

static std::vector<Task> build_tasks() {
    std::vector<Task> t;
    Task k;

    k = {"Temporales del usuario", false, false, [](Ctx& c) {
        wipe_contents(env(L"TEMP"), c);
        wipe_contents(join(env(L"LOCALAPPDATA"), L"Temp"), c);
    }}; t.push_back(k);

    k = {"Temporales de Windows", true, false, [](Ctx& c) {
        wipe_contents(join(env(L"WINDIR"), L"Temp"), c);
    }}; t.push_back(k);

    k = {"Prefetch", true, false, [](Ctx& c) {
        wipe_contents(join(env(L"WINDIR"), L"Prefetch"), c);
    }}; t.push_back(k);

    k = {"Cache de miniaturas e iconos", false, false, [](Ctx& c) {
        wipe_if(join(env(L"LOCALAPPDATA"), L"Microsoft\\Windows\\Explorer"), c,
                [](const std::wstring& n) {
                    return starts_ci(n, L"thumbcache_") || starts_ci(n, L"iconcache_");
                });
    }}; t.push_back(k);

    k = {"Archivos recientes", false, false, [](Ctx& c) {
        wipe_contents(join(env(L"APPDATA"), L"Microsoft\\Windows\\Recent"), c);
    }}; t.push_back(k);

    k = {"Informes de errores (WER)", false, false, [](Ctx& c) {
        wipe_contents(join(env(L"LOCALAPPDATA"), L"Microsoft\\Windows\\WER"), c);
        wipe_contents(join(env(L"PROGRAMDATA"), L"Microsoft\\Windows\\WER"), c);
    }}; t.push_back(k);

    k = {"Volcados de memoria (dumps)", false, false, [](Ctx& c) {
        wipe_contents(join(env(L"LOCALAPPDATA"), L"CrashDumps"), c);
        wipe_contents(join(env(L"WINDIR"), L"Minidump"), c);
        wipe(join(env(L"WINDIR"), L"MEMORY.DMP"), c, true);
    }}; t.push_back(k);

    k = {"Logs de Windows (CBS/Panther)", true, false, [](Ctx& c) {
        wipe_if(join(env(L"WINDIR"), L"Logs\\CBS"), c, [](const std::wstring& n) {
            return ends_ci(n, L".log") || ends_ci(n, L".cab");
        });
        wipe_if(join(env(L"WINDIR"), L"Panther"), c, [](const std::wstring& n) {
            return ends_ci(n, L".log");
        });
    }}; t.push_back(k);

    k = {"Cache de Windows Update", true, false, [](Ctx& c) {
        if (!c.dry) { run_cmd(L"net stop wuauserv"); run_cmd(L"net stop bits"); }
        wipe_contents(join(env(L"WINDIR"), L"SoftwareDistribution\\Download"), c);
        if (!c.dry) { run_cmd(L"net start bits"); run_cmd(L"net start wuauserv"); }
    }}; t.push_back(k);

    k = {"Delivery Optimization", true, false, [](Ctx& c) {
        wipe_contents(join(env(L"WINDIR"),
            L"ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache"), c);
    }}; t.push_back(k);

    k = {"Navegadores (Chrome/Edge/Brave/Opera)", false, false, [](Ctx& c) {
        std::wstring l = env(L"LOCALAPPDATA"), r = env(L"APPDATA");
        clean_chromium(join(l, L"Google\\Chrome\\User Data"), c);
        clean_chromium(join(l, L"Microsoft\\Edge\\User Data"), c);
        clean_chromium(join(l, L"BraveSoftware\\Brave-Browser\\User Data"), c);
        clean_chromium(join(l, L"Vivaldi\\User Data"), c);
        clean_chromium(join(r, L"Opera Software\\Opera Stable"), c);
    }}; t.push_back(k);

    k = {"Cache de Firefox", false, false, [](Ctx& c) {
        std::wstring base = join(env(L"LOCALAPPDATA"), L"Mozilla\\Firefox\\Profiles");
        if (base.empty()) return;
        std::vector<std::wstring> kids;
        list_dir(base, kids);
        for (size_t i = 0; i < kids.size(); ++i) {
            wipe_contents(base + L"\\" + kids[i] + L"\\cache2", c);
            wipe_contents(base + L"\\" + kids[i] + L"\\startupCache", c);
        }
    }}; t.push_back(k);

    k = {"Papelera de reciclaje", false, false, [](Ctx& c) { recycle_bin(c); }};
    t.push_back(k);

    k = {"Cache DNS", false, false, [](Ctx& c) {
        if (!c.dry) run_cmd(L"ipconfig /flushdns");
    }}; t.push_back(k);

    k = {"Portapapeles", false, false, [](Ctx& c) {
        if (!c.dry && OpenClipboard(NULL)) { EmptyClipboard(); CloseClipboard(); }
    }}; t.push_back(k);

    k = {"Historial de Win+R (Ejecutar)", false, false, [](Ctx& c) {
        reg_clear_values(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU", c);
    }}; t.push_back(k);

    k = {"Historial de rutas y busquedas del Explorador", false, false, [](Ctx& c) {
        reg_clear_values(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths", c);
        reg_clear_values(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\WordWheelQuery", c);
    }}; t.push_back(k);

    k = {"Liberador de espacio de Windows (cleanmgr)", true, false, [](Ctx& c) {
        run_cleanmgr(c);
    }}; t.push_back(k);

    // ---- Profundas ----
    k = {"Visor de eventos (todos los logs)", true, true, [](Ctx& c) {
        if (!c.dry)
            run_cmd(L"for /F \"tokens=*\" %G in ('wevtutil el') do wevtutil cl \"%G\"");
    }}; t.push_back(k);

    k = {"Cache de fuentes y Microsoft Store", true, true, [](Ctx& c) {
        if (!c.dry) run_cmd(L"net stop FontCache");
        wipe_if(join(env(L"WINDIR"), L"ServiceProfiles\\LocalService\\AppData\\Local\\FontCache"), c,
                [](const std::wstring& n) { return starts_ci(n, L"~FontCache") || ends_ci(n, L".dat"); });
        if (!c.dry) run_cmd(L"net start FontCache");
        wipe_contents(join(env(L"LOCALAPPDATA"),
            L"Packages\\Microsoft.WindowsStore_8wekyb3d8bbwe\\LocalCache"), c);
    }}; t.push_back(k);

    k = {"DISM limpieza de componentes (lento)", true, true, [](Ctx& c) {
        if (!c.dry) run_cmd(L"DISM /Online /Cleanup-Image /StartComponentCleanup");
    }}; t.push_back(k);

    return t;
}

// ----------------------------------------------------------------------------
//  Interfaz
// ----------------------------------------------------------------------------
struct Glyph { const char* r[6]; };

// Dibujo en "ASCII" que se convierte a bloques al imprimir (no depende del compilador):
//  # = bloque lleno   a = esquina sup-der   b = esquina sup-izq
//  c = esquina inf-der  d = esquina inf-izq  e = linea horizontal  f = linea vertical
static const Glyph gD = {{"######a ", "##bee##a", "##f  ##f", "##f  ##f", "######bc", "deeeeec "}};
static const Glyph gI = {{"##a", "##f", "##f", "##f", "##f", "dec"}};
static const Glyph gS = {{"#######a", "##beeeec", "#######a", "deeee##f", "#######f", "deeeeeec"}};
static const Glyph gK = {{"##a  ##a", "##f ##bc", "#####bc ", "##be##a ", "##f  ##a", "dec  dec"}};
static const Glyph gW = {{"##a    ##a", "##f    ##f", "##f #a ##f", "##f###a##f", "d###b###bc", " deecdeec "}};
static const Glyph gE = {{"#######a", "##beeeec", "#####a  ", "##beec  ", "#######a", "deeeeeec"}};
static const Glyph gP = {{"######a ", "##bee##a", "######bc", "##beeec ", "##f     ", "dec     "}};

static void emit_art(const std::string& row) {
    std::cout << std::flush;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) { std::cout << row; return; }   // salida redirigida
    std::wstring w;
    for (size_t i = 0; i < row.size(); ++i) {
        switch (row[i]) {
            case '#': w += L'\u2588'; break;
            case 'a': w += L'\u2557'; break;
            case 'b': w += L'\u2554'; break;
            case 'c': w += L'\u255D'; break;
            case 'd': w += L'\u255A'; break;
            case 'e': w += L'\u2550'; break;
            case 'f': w += L'\u2551'; break;
            default:  w += (wchar_t)row[i];
        }
    }
    DWORD n = 0;
    WriteConsoleW(h, w.c_str(), (DWORD)w.size(), &n, NULL);
}

static void draw_word(const Glyph* const* word, size_t count, const char* const* colors) {
    for (int row = 0; row < 6; ++row) {
        std::string line;
        for (size_t w = 0; w < count; ++w) line += word[w]->r[row];
        std::cout << "  " << colors[row] << cBold;
        emit_art(line);
        std::cout << cReset << "\n" << std::flush;
        Sleep(45);
    }
}

static void banner() {
    const Glyph* disk[]  = {&gD, &gI, &gS, &gK};
    const Glyph* sweep[] = {&gS, &gW, &gE, &gE, &gP};
    const char* c1[] = {cCyan, cCyan, cCyan, cBlue, cBlue, cBlue};
    const char* c2[] = {cMagenta, cMagenta, cMagenta, cBlue, cBlue, cCyan};

    std::cout << "\n";
    draw_word(disk, 4, c1);
    draw_word(sweep, 5, c2);
    std::cout << "\n" << cGray
              << "  ----------------------------------------------------------\n"
              << "   " APP_TAGLINE "  |  v" APP_VERSION "\n"
              << "  ----------------------------------------------------------"
              << cReset << "\n\n";
}

static void typewriter(const std::string& s, const char* color) {
    std::cout << color << cBold;
    for (size_t i = 0; i < s.size(); ++i) { std::cout << s[i] << std::flush; Sleep(22); }
    std::cout << cReset << "\n";
}

static std::string bar(size_t done, size_t total, int width = 24) {
    int filled = total ? (int)(width * done / total) : 0;
    std::string s = "[";
    for (int i = 0; i < width; ++i) s += (i < filled ? '#' : '-');
    return s + "]";
}

static void wait_enter() {
    std::cout << cGray << "\n  Presiona Enter para salir..." << cReset;
    std::cin.clear();
    std::string d;
    std::getline(std::cin, d);
}

static std::string pad_to(const std::string& s, size_t w, char ch) {
    return s.size() >= w ? s : s + std::string(w - s.size(), ch);
}

static void row(const std::string& k, const std::string& v, const char* col) {
    std::string line = k + v;
    std::cout << cGray << "  | " << cReset << k << col << cBold << v << cReset
              << std::string(line.size() < 43 ? 43 - line.size() : 0, ' ')
              << cGray << " |\n" << cReset;
}

int main(int argc, char** argv) {
    enable_console();
    set_window_icon();

    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-n" || a == "--dry-run") opt.dry = true;
        else if (a == "-d" || a == "--deep") opt.deep = true;
        else if (a == "-y" || a == "--yes") opt.yes = true;
        else if (a == "--no-pause") opt.pause = false;
        else if (a == "-v" || a == "--version") {
            std::cout << APP_NAME " " APP_VERSION "\n";
            return 0;
        }
        else if (a == "-h" || a == "--help" || a == "/?") {
            std::cout << APP_NAME " v" APP_VERSION " - " APP_TAGLINE "\n\n"
                         "  -n, --dry-run   Simular (no borra nada)\n"
                         "  -d, --deep      Limpieza profunda\n"
                         "  -y, --yes       No pedir confirmacion\n"
                         "      --no-pause  No esperar Enter al final\n"
                         "  -v, --version   Mostrar la version\n"
                         "  -h, --help      Ayuda\n";
            return 0;
        }
    }

    hide_cursor(true);
    banner();
    typewriter("Liberando espacio en disco...", cGreen);
    std::cout << "\n";

    const bool admin = is_admin();
    std::cout << "  " << cGray << "Modo        : " << cReset
              << (opt.dry ? (std::string(cYellow) + "SIMULACION (no se borra nada)")
                          : (std::string(cGreen) + "LIMPIEZA REAL")) << cReset << "\n";
    std::cout << "  " << cGray << "Profundidad : " << cReset << cCyan
              << (opt.deep ? "profunda" : "estandar") << cReset << "\n";
    std::cout << "  " << cGray << "Privilegios : " << cReset
              << (admin ? (std::string(cGreen) + "Administrador")
                        : (std::string(cYellow) + "Usuario (se omiten tareas de sistema)"))
              << cReset << "\n\n";

    if (!admin && !opt.yes) {
        std::cout << "  " << cYellow << "?" << cReset
                  << " Reiniciar como Administrador para limpiar todo? [s/N]: ";
        std::string r; std::getline(std::cin, r);
        if (!r.empty() && (r[0] == 's' || r[0] == 'S' || r[0] == 'y' || r[0] == 'Y')) {
            relaunch_as_admin(opt);
            hide_cursor(false);
            return 0;
        }
    }
    if (!opt.yes) {
        std::cout << "  " << cYellow << "?" << cReset << " Continuar con la limpieza? [S/n]: ";
        std::string r; std::getline(std::cin, r);
        if (!r.empty() && (r[0] == 'n' || r[0] == 'N')) {
            std::cout << cRed << "\n  Cancelado." << cReset << "\n";
            hide_cursor(false);
            return 0;
        }
    }
    std::cout << "\n";

    std::vector<Task> all = build_tasks(), tasks;
    for (size_t i = 0; i < all.size(); ++i)
        if (!all[i].deep_only || opt.deep) tasks.push_back(all[i]);

    Ctx total; total.dry = opt.dry;
    size_t skipped = 0;
    DWORD t0 = GetTickCount();
    u64 free_before = free_space();

    for (size_t i = 0; i < tasks.size(); ++i) {
        Task& t = tasks[i];
        std::string prefix = "[" + std::to_string(i + 1) + "/" + std::to_string(tasks.size()) + "] ";
        std::string dots = pad_to(t.name + " ", 44, '.');

        if (t.needs_admin && !admin) {
            std::cout << "  " << cYellow << "[!!]" << cReset << " " << cGray << prefix << cReset
                      << dots << cGray << " omitido (requiere admin)" << cReset << "\n";
            ++skipped;
            continue;
        }

        g_label = prefix + t.name;
        spin();

        Ctx local; local.dry = opt.dry;
        t.run(local);

        total.bytes += local.bytes; total.files += local.files; total.errors += local.errors;

        clear_line();
        std::cout << "  " << cGreen << "[OK]" << cReset << " " << cGray << prefix << cReset
                  << dots << " " << cCyan << cBold << human(local.bytes) << cReset
                  << cGray << " (" << local.files << " elem.)  " << bar(i + 1, tasks.size(), 10)
                  << " " << ((i + 1) * 100 / tasks.size()) << "%" << cReset << "\n" << std::flush;
    }

    u64 free_after = free_space();
    double secs = (GetTickCount() - t0) / 1000.0;
    char tb[32]; std::snprintf(tb, sizeof tb, "%.1f s", secs);

    std::cout << "\n" << cGray
              << "  +---------------------------------------------+\n"
              << "  |" << cReset << cBold << cGreen
              << "            LIMPIEZA COMPLETADA              " << cReset << cGray << "|\n"
              << "  +---------------------------------------------+\n" << cReset;
    row(opt.dry ? "Se liberarian    : " : "Espacio liberado  : ", human(total.bytes), cCyan);
    row("Elementos         : ", std::to_string(total.files), cGreen);
    row("En uso / omitidos : ", std::to_string(total.errors), cYellow);
    row("Tareas omitidas   : ", std::to_string(skipped), cYellow);
    row("Tiempo            : ", tb, cMagenta);
    std::cout << cGray << "  +---------------------------------------------+\n" << cReset;
    row("Libre antes       : ", human(free_before), cGray);
    row("Libre despues     : ", human(free_after), cGray);
    row("Diferencia real   : ", free_after > free_before ? human(free_after - free_before) : std::string("0 B"), cGreen);
    std::cout << cGray << "  +---------------------------------------------+" << cReset << "\n";

    if (opt.dry)
        std::cout << "\n" << cYellow
                  << "  Fue una simulacion: no se borro nada. Ejecuta sin -n para limpiar."
                  << cReset << "\n";

    hide_cursor(false);
    if (opt.pause) wait_enter();
    return 0;
}
