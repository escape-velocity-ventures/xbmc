set(SOURCES Album.cpp
            Artist.cpp
            ContextMenus.cpp
            GUIViewStateMusic.cpp
            MusicCRUDRepository.cpp
            MusicDatabase.cpp
            MusicDatasetHelper.cpp
            MusicDbUrl.cpp
            MusicEmbeddedImageFileLoader.cpp
            MusicFileItemClassify.cpp
            MusicInfoLoader.cpp
            MusicLibraryQueue.cpp
            MusicMaintenanceService.cpp
            MusicNavRepository.cpp
            MusicPlaylistService.cpp
            MusicQueryBuilder.cpp
            MusicSchemaManager.cpp
            MusicSearchService.cpp
            MusicThumbLoader.cpp
            MusicUtils.cpp
            Song.cpp)

set(HEADERS Album.h
            Artist.h
            ContextMenus.h
            GUIViewStateMusic.h
            MusicCRUDRepository.h
            MusicDatabase.h
            MusicDatasetHelper.h
            MusicDbUrl.h
            MusicEmbeddedImageFileLoader.h
            MusicFileItemClassify.h
            MusicInfoLoader.h
            MusicLibraryQueue.h
            MusicMaintenanceService.h
            MusicNavRepository.h
            MusicPlaylistService.h
            MusicQueryBuilder.h
            MusicSchemaManager.h
            MusicSearchService.h
            MusicThumbLoader.h
            MusicUtils.h
            Song.h)

core_add_library(music)
