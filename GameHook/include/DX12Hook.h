#pragma once

#include "pch.h"

bool initDX12VTable();
DWORD WINAPI hookDX12VTable();
void unhookDX12VTable();