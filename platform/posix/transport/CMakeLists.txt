# Include filepaths for source and include.
include( ${PLATFORM_DIR}/posix/posixFilePaths.cmake )

set( TRANSPORT_INTERFACE_INCLUDE_DIR
     ${MODULES_DIR}/standard/coreMQTT/source/interface )

# Set path to corePKCS11 and it's third party libraries.
set(COREPKCS11_LOCATION "${CMAKE_SOURCE_DIR}/libraries/standard/corePKCS11")
set(CORE_PKCS11_3RDPARTY_LOCATION "${COREPKCS11_LOCATION}/source/dependency/3rdparty")

# Include PKCS #11 library's source and header path variables.
include( ${COREPKCS11_LOCATION}/pkcsFilePaths.cmake )

list(APPEND PKCS_SOURCES
    "${COREPKCS11_LOCATION}/source/portable/os/posix/core_pkcs11_pal.c"
    "${COREPKCS11_LOCATION}/source/portable/os/core_pkcs11_pal_utils.c"
    "${CORE_PKCS11_3RDPARTY_LOCATION}/mbedtls_utils/mbedtls_utils.c"
)

# Create target for POSIX implementation of MbedTLS transport with PKCS #11.
add_library( transport_mbedtls_pkcs11_posix
                ${MBEDTLS_PKCS11_TRANSPORT_SOURCES}
                ${PKCS_SOURCES} )

target_link_libraries( transport_mbedtls_pkcs11_posix
                       PRIVATE
                          mbedtls )

target_include_directories(
    transport_mbedtls_pkcs11_posix
    PUBLIC
        ${COMMON_TRANSPORT_INCLUDE_PUBLIC_DIRS}
        ${LOGGING_INCLUDE_DIRS}
        ${TRANSPORT_INTERFACE_INCLUDE_DIR}
        ${PKCS_INCLUDE_PUBLIC_DIRS}
        "${COREPKCS11_LOCATION}/source/portable/os"
        ${CORE_PKCS11_3RDPARTY_LOCATION}/pkcs11
        ${DEMOS_DIR}/fleet_provisioning/fleet_provisioning_keys_cert
    PRIVATE
        ${CORE_PKCS11_3RDPARTY_LOCATION}/mbedtls_utils
)
