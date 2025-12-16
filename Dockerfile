FROM ghcr.io/wiiu-env/devkitppc:20241128

# Copy WUPS (Wii U Plugin System), libnotifications, and libmappedmemory from their artifact images
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:latest /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libmappedmemory:20230621 /artifacts $DEVKITPRO

# Install libgd and dependencies for image loading
RUN dkp-pacman -Sy --noconfirm --overwrite '*' ppc-libgd ppc-libpng ppc-libjpeg-turbo ppc-zlib

COPY --chown=developer:developer . /project
WORKDIR /project
RUN make -j$(nproc)
