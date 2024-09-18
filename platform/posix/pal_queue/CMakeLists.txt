# Include filepaths for source and include.
include( ${PLATFORM_DIR}/posix/posixFilePaths.cmake )

# Create target for POSIX implementation of MbedTLS transport with PKCS #11.
add_library( pal_queue
                ${PAL_QUEUE_SOURCES} )
