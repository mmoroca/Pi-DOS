#ifndef PI_DOS_VIDEO_TRANSACTION_H
#define PI_DOS_VIDEO_TRANSACTION_H
/* Commit three prepared INIs on one filesystem; restore names on any failure.
 * Returns 1 on success, 0 on failure with rollback, -1 on rollback failure. */
int pv_commit(const char *windows_directory, const char *staging_directory);
#endif
