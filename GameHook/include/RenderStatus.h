#pragma once

#include "pch.h"

void initRenderStatus();
void setRenderStatus(int flag);
int getRenderStatus();
void waitRenderStatus(int flag);
void wakeRender();

    
