SUMMARY = "Deterministic sensor telemetry supervisor"
DESCRIPTION = "Validates versioned telemetry frames, CRC values, sequence continuity, and safety states."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=c4b4c7c8aee5e745dfdc8eaeb6e983c3"

SRC_URI = " \
    file://rtsafe-validate.service \
    file://rtsafe-validate.path \
"

inherit cmake externalsrc systemd

# Monorepo development build. A release recipe should replace externalsrc with
# a checksummed source archive once a public release location exists.
EXTERNALSRC = "${THISDIR}/../../../.."
EXTERNALSRC_BUILD = "${WORKDIR}/build"

EXTRA_OECMAKE = " \
    -DRTSAFE_BUILD_TESTS=OFF \
    -DRTSAFE_BUILD_SIMULATOR=OFF \
    -DRTSAFE_ENABLE_STRICT_WARNINGS=ON \
"

SYSTEMD_SERVICE:${PN} = "rtsafe-validate.path rtsafe-validate.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/rtsafe-validate.service ${D}${systemd_system_unitdir}/
    install -m 0644 ${WORKDIR}/rtsafe-validate.path ${D}${systemd_system_unitdir}/
}
