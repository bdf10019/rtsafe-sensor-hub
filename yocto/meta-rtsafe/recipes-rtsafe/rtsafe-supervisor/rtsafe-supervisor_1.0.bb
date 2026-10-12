SUMMARY = "Deterministic sensor telemetry supervisor"
DESCRIPTION = "Validates versioned telemetry frames, CRC values, sequence continuity, and safety states."
# No general open-source license is granted for current original code.
# See LICENSE_INFORMATION.md; upstream component licenses remain unchanged.
LICENSE = "CLOSED"

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
