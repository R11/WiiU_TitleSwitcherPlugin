FROM ghcr.io/wiiu-env/devkitppc:20241128

# Copy WUPS (Wii U Plugin System) and libnotifications from their artifact images
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20240505 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20240426 /artifacts $DEVKITPRO

COPY --chown=developer:developer . /project
WORKDIR /project
RUN make -j$(nproc)
