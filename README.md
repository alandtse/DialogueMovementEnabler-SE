# Dialogue Movement Enabler

SKSE/SKSEVR plugin that allows you to move around in Dialogue Menus.
*	[SSE/AE Version](https://www.nexusmods.com/skyrimspecialedition/mods/43708)
*	[VR Version](https://www.nexusmods.com/skyrimspecialedition/mods/59816)

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++
* [CommonLibSSE](https://github.com/SniffleMan/CommonLibSSE)
	* Add this as as an environment variable `CommonLibSSEPath` or use wrapper
* [CommonLibVR](https://github.com/alandtse/CommonLibVR/tree/vr)
	* Add this as as an environment variable `CommonLibVRPath`

## User Requirements
* [SKSE64/VR](https://skse.silverlock.org/)
* [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
	* Needed for AE
* [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
	* Needed for VR

## Register Visual Studio as a Generator
* Open `x64 Native Tools Command Prompt`
* Run `cmake`
* Close the cmd window

## Building

```
git clone https://github.com/alandtse/DialogueMovementEnabler-SE.git
cd DialogueMovementEnabler-SE
```
### CommonLibSSE/CommonLibVR
```
# pull CommonLibSSE and CommonLibVR
# alternatively, do not pull and set environment variable `CommonLibSSEPath` or `CommonLibVRPath` if you need something different from external

git submodule update --init --recursive

```
### AE
```
cmake --preset vs2022-windows-vcpkg-ae
cmake --build build --config Release
```

### SSE
```
cmake --preset vs2022-windows-vcpkg
cmake --build build --config Release
```
### VR
```
cmake --preset vs2022-windows-vcpkg-vr
cmake --build buildvr --config Release
```

## License
[MIT](LICENSE)

# Credits
 * Vermunds - Original code