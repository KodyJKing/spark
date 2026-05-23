/*
 * An ESP util for finding vectors in memory.
 */

#pragma once

#include <Windows.h>

namespace Mod::DevTools::VectorProfiler {
	void start(DWORD threadId);
	void stop();
	void render();
}

