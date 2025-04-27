/**
 * @file restrict.cpp
 *
 * Implementation of functionality for checking if the game will be able run on the system.
 */

#include "appfat.h"
#include "utils/file_util.h"
#include "utils/paths.h"

#include <SDL.h>

namespace devilution {

void ReadOnlyTest()
{
	const std::string path = paths::PrefPath() + "Diablo1ReadOnlyTest.foo";
	SDL_RWops *file = SDL_RWFromFile(path.c_str(), "w");
	if (file == nullptr) {
#ifdef __DREAMCAST__
		// Check if the error is related to VMU paths and we're in suppression mode
		extern bool suppressVmuErrors;
		if (suppressVmuErrors && paths::PrefPath().find("/vmu") != std::string::npos) {
			// Skip showing the error dialog for VMU paths while we're trying them
			return;
		}
#endif
		DirErrorDlg(paths::PrefPath());
	}

	SDL_RWclose(file);
	RemoveFile(path.c_str());
}

} // namespace devilution
