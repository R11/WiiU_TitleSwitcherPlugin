/**
 * Details View Implementation
 */

#include "details_view.h"
#include <algorithm>

namespace DetailsView {

Layout ComputeLayout(const Screen::Descriptor& screen) {
    Layout layout;

    // Store content area bounds
    layout.contentX = screen.contentX();
    layout.contentY = screen.contentY();
    layout.contentWidth = screen.contentWidth();
    layout.contentHeight = screen.contentHeight();

    // Artwork size: proportional to screen height, capped at reasonable max
    int artworkBase = static_cast<int>(screen.contentHeight() * 0.55f);
    int maxArtwork = screen.scaled(300);
    layout.artwork.size = std::min(artworkBase, maxArtwork);

    // Position artwork on left, vertically centered
    layout.artwork.x = layout.contentX + screen.scaled(40);
    layout.artwork.y = layout.contentY +
                       (layout.contentHeight - layout.artwork.size) / 2;

    // Title and info positioned to the right of artwork
    int infoStartX = layout.artwork.x + layout.artwork.size + screen.scaled(32);

    // Title area
    layout.title.x = infoStartX;
    layout.title.y = layout.artwork.y;
    layout.title.fontSize = screen.scaled(20);
    layout.title.maxWidth = screen.width - infoStartX - screen.marginX;

    // Divider under title
    layout.divider.x = layout.title.x;
    layout.divider.y = layout.title.y + layout.title.fontSize + screen.scaled(8);
    layout.divider.width = layout.title.maxWidth;

    // Info section below divider
    layout.info.x = infoStartX;
    layout.info.y = layout.divider.y + screen.scaled(16);
    layout.info.lineHeight = screen.scaled(22);
    layout.info.labelWidth = screen.scaled(100);
    layout.info.valueX = layout.info.x + layout.info.labelWidth + screen.scaled(12);

    // Calculate how many info rows fit
    int artworkBottom = layout.artwork.y + layout.artwork.size;
    int availableInfoHeight = artworkBottom - layout.info.y;
    layout.info.maxRows = availableInfoHeight / layout.info.lineHeight;
    if (layout.info.maxRows < 1) layout.info.maxRows = 1;

    // Button hints at bottom
    layout.hints.y = screen.height - screen.marginY - screen.scaled(24);
    layout.hints.fontSize = screen.scaled(14);

    return layout;
}

} // namespace DetailsView
