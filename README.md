<div align="center">

<img src="assets/logo.svg" alt="DiskSweep" width="160"/>

# DiskSweep


<a href="https://github.com/MR-Rodolfo/liebradordeDiscoSystema/releases/tag/DiskSweep2.0.0full">Releases</a>



**Liberador de espacio en disco para Windows**
Rápido, ligero y sin dependencias. Un solo `.exe`.

![Versión](https://img.shields.io/badge/versi%C3%B3n-2.0.0-2979FF?style=for-the-badge)
![Plataforma](https://img.shields.io/badge/Windows-10%20%7C%2011-00E5FF?style=for-the-badge&logo=windows&logoColor=white)
![Lenguaje](https://img.shields.io/badge/C%2B%2B-11-0F2027?style=for-the-badge&logo=cplusplus&logoColor=white)
![Licencia](https://img.shields.io/badge/licencia-MIT-4CAF50?style=for-the-badge)

</div>

---

## ✨ ¿Qué hace?

DiskSweep recorre las zonas de tu sistema donde se acumula basura y recupera espacio en disco, con una interfaz de terminal animada y un resumen claro al final.

| Categoría | Qué limpia |
|---|---|
| 🗂️ **Temporales** | Carpeta `Temp` del usuario y de Windows, Prefetch |
| 🖼️ **Cachés** | Miniaturas, iconos, DNS, shaders |
| 🌐 **Navegadores** | Chrome, Edge, Brave, Vivaldi, Opera y Firefox (solo caché) |
| 🐞 **Diagnóstico** | Informes de errores (WER), volcados de memoria, logs CBS/Panther |
| 🔄 **Windows Update** | Descargas antiguas y Delivery Optimization |
| 🕘 **Historial** | Archivos recientes, Win+R, rutas y búsquedas del Explorador, portapapeles |
| 🗑️ **Papelera** | Vaciado de la papelera de reciclaje |
| 🧹 **Sistema** | Liberador de espacio de Windows (`cleanmgr`) |

### Modo profundo (`--deep`)
- Visor de eventos (todos los registros)
- Caché de fuentes y de Microsoft Store
- Limpieza de componentes con DISM

## 🚀 Uso

```bat
DiskSweep.exe            :: limpieza estándar con confirmación
DiskSweep.exe -n         :: simulación, no borra nada
DiskSweep.exe -d         :: limpieza profunda
DiskSweep.exe -y         :: sin preguntar
```

| Opción | Descripción |
|---|---|
| `-n`, `--dry-run` | Simula la limpieza y calcula cuánto se liberaría |
| `-d`, `--deep` | Activa la limpieza profunda |
| `-y`, `--yes` | No pide confirmación |
| `--no-pause` | No espera Enter al terminar |
| `-v`, `--version` | Muestra la versión |
| `-h`, `--help` | Muestra la ayuda |

> 💡 Ejecútalo como **Administrador** para incluir las tareas del sistema. Si no lo haces, DiskSweep te ofrece reiniciarse con permisos elevados.

## 🛡️ Seguridad

- No toca la carpeta **Descargas** ni **Windows.old**.
- Omite archivos en uso o sin permisos, sin forzar nada.
- No sigue enlaces simbólicos ni puntos de unión.
- Usa siempre `-n` primero si quieres ver qué se borraría.

## 🔧 Compilar

No necesita librerías externas, solo `<windows.h>` y la biblioteca estándar.

Lo más fácil: ejecuta **`build.bat`**. Incrusta `assets/logo.ico` en el `.exe` y compila.

A mano:

```bat
:: MinGW / g++
windres DiskSweep.rc -O coff -o DiskSweep_res.o
g++ DiskSweep.cpp DiskSweep_res.o -o DiskSweep.exe -static

:: MSVC
rc DiskSweep.rc
cl /EHsc /O2 DiskSweep.cpp DiskSweep.res
```

El `.exe` resultante lleva el ícono de DiskSweep (en el Explorador, la barra de tareas y la ventana).

## 📁 Estructura

```
DiskSweep/
├── DiskSweep.cpp      # Código fuente
├── DiskSweep.rc       # Ícono e info de versión del .exe
├── build.bat          # Compilación rápida
├── assets/            # Logo (SVG) e ícono (logo.ico)
├── CHANGELOG.md
└── LICENSE
```

## 📝 Historial

Consulta el [CHANGELOG](CHANGELOG.md). DiskSweep nació como *GrifoCleaner v1.0*.

## 📄 Licencia

Distribuido bajo licencia [MIT](LICENSE).

<div align="center">

<sub>Hecho con 💙 por <a href="https://github.com/MR-Rodolfo">MR-Rodolfo</a></sub>

</div>
