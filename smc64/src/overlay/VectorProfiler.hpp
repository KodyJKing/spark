/*
 * An ESP util for finding vectors in memory.
 */

#pragma once

#include <Windows.h>

namespace Overlay::ESP::VectorProfiler {
	void start(DWORD threadId);
	void stop();
	void render();
}

