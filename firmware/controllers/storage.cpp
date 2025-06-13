/**
 * @file    storage.cpp
 * @brief   adapter for unify access to MFS and legacy 'internal flash' storage drivers
 *
 * @date Jan 4, 2025
 * @author Andrey Gusakov
 */

#include "pch.h"

#include "storage.h"

/* If any setting storage is exist */
#if EFI_CONFIGURATION_STORAGE

#if EFI_STORAGE_MFS == TRUE
#include "mfs_storage.h"

StorageStatus storageWrite(int id, const uint8_t *ptr, size_t size) {
	return mfsStorageWrite(id, ptr, size);
}

StorageStatus storageRead(int id, uint8_t *ptr, size_t size) {
	return mfsStorageRead(id, ptr, size);
}

void initStorage() {
	initStorageMfs();
}
#else
StorageStatus storageWrite(int, const uint8_t*, size_t) {
	return StorageStatus::Failed;
}

StorageStatus storageRead(int, uint8_t*, size_t) {
	return StorageStatus::NotFound;
}

void initStorage() { }
#endif

#endif // EFI_CONFIGURATION_STORAGE
