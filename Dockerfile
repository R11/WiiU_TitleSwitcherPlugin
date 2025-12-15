FROM ghcr.io/wiiu-env/devkitppc:20241128

# Copy WUPS (Wii U Plugin System) and libnotifications from their artifact images
# Try latest tag to get overlay support
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:latest /artifacts $DEVKITPRO

COPY --chown=developer:developer . /project
WORKDIR /project
RUN make -j$(nproc)
