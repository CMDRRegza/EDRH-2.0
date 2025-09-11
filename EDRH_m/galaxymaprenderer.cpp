#include "galaxymaprenderer.h"
#include "exceptionmanager.h"
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QFont>
#include <QPixmap>
#include <QDebug>
#include <QtMath>
#include <QDir>
#include <QRandomGenerator>
#include <QTimer>
#include <QMouseEvent>
#include <QHoverEvent>

GalaxyMapRenderer::GalaxyMapRenderer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_zoomLevel(1.0)
    , m_panOffset(0, 0)
    , m_showMainSequence(true)
    , m_showNeutronStars(true)
    , m_showWhiteDwarfs(true)
    , m_showBlackHoles(true)
    , m_tooltipsEnabled(true)
    , m_backgroundLoaded(false)
    , m_hoveredSystem(nullptr)
    , m_commanderPosition(0, 0)
    , m_showAllCommanders(false)
    , m_isAdminMode(false)
    , m_lastMousePosition(0, 0)
{
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing);
    
    // Enable mouse and hover events
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    
    // Try to load the background image
    setBackgroundImage("assets/E47CDFX.png");
    
    // Load real systems from database when component is complete
    // Note: Real systems will be loaded via setStarSystems() when data arrives from SupabaseClient
    
    // Handle resizing by monitoring width/height changes
    connect(this, &QQuickItem::widthChanged, this, [this]() {
        QTimer::singleShot(10, this, [this]() {
            if (!m_realStars.isEmpty()) {
                repositionStarsAfterResize();
            }
            update();
        });
    });
    
    connect(this, &QQuickItem::heightChanged, this, [this]() {
        QTimer::singleShot(10, this, [this]() {
            if (!m_realStars.isEmpty()) {
                repositionStarsAfterResize();
            }
            update();
        });
    });
}

void GalaxyMapRenderer::paint(QPainter *painter)
{
    ExceptionManager::instance().safeCatch("GalaxyMapRenderer::paint", [&]() {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // Clear with black space
    painter->fillRect(boundingRect(), QColor(0, 0, 0));
    
    // Set up the world-to-screen transformation
    painter->save();
    painter->translate(width() / 2.0, height() / 2.0);
    painter->translate(m_panOffset.x(), m_panOffset.y());
    painter->scale(m_zoomLevel, m_zoomLevel);
    painter->translate(-width() / 2.0, -height() / 2.0);
    
    // Draw background
    drawBackground(painter);
    
    // Draw stars
    drawStars(painter);
    
    painter->restore();
    
    // Draw UI elements (not transformed)
    drawUI(painter);
    });
}

void GalaxyMapRenderer::drawBackground(QPainter *painter)
{
    // Background rendering
    if (m_backgroundLoaded && !m_backgroundPixmap.isNull()) {
        // Draw the galaxy background image with proper aspect ratio
        QRectF targetRect(0, 0, width(), height());
        QRectF sourceRect = m_backgroundPixmap.rect();
        
        // Calculate aspect ratios
        qreal imageAspect = sourceRect.width() / sourceRect.height();
        qreal canvasAspect = width() / height();
        
        QRectF drawRect;
        if (imageAspect > canvasAspect) {
            // Image is wider - fit to height, center horizontally
            qreal drawWidth = height() * imageAspect;
            qreal drawHeight = height();
            drawRect = QRectF((width() - drawWidth) / 2, 0, drawWidth, drawHeight);
        } else {
            // Image is taller - fit to width, center vertically
            qreal drawWidth = width();
            qreal drawHeight = width() / imageAspect;
            drawRect = QRectF(0, (height() - drawHeight) / 2, drawWidth, drawHeight);
        }
        
        painter->drawPixmap(drawRect, m_backgroundPixmap, sourceRect);
    } else {
        // Draw enhanced procedural galaxy background with better visibility
        QRadialGradient gradient(width() / 2.0, height() / 2.0, qMax(width(), height()) / 2.0);
        gradient.setColorAt(0.0, QColor(120, 140, 180));   // Brighter center
        gradient.setColorAt(0.3, QColor(80, 90, 120));     // Lighter mid tone
        gradient.setColorAt(0.7, QColor(40, 50, 80));      // Lighter dark edge
        gradient.setColorAt(1.0, QColor(20, 25, 45));      // Lighter space black
        
        painter->fillRect(0, 0, width(), height(), QBrush(gradient));
        
        // Add enhanced nebula-like effects for better visibility
        painter->setOpacity(0.5);
        for (int i = 0; i < 25; ++i) {
            qreal x = (i * 123.45) * width() / 1000.0;
            qreal y = (i * 234.56) * height() / 1000.0;
            qreal radius = 80 + (i * 17) % 150;
            
            QRadialGradient nebula(x, y, radius);
            QColor nebulaColor = (i % 4 == 0) ? QColor(120, 80, 180, 120) :
                                (i % 4 == 1) ? QColor(180, 100, 140, 120) :
                                (i % 4 == 2) ? QColor(140, 100, 180, 120) :
                                               QColor(80, 120, 180, 120);
            
            nebula.setColorAt(0.0, nebulaColor);
            nebula.setColorAt(1.0, QColor(0, 0, 0, 0));
            
            painter->fillRect(x - radius, y - radius, radius * 2, radius * 2, QBrush(nebula));
        }
        painter->setOpacity(1.0);
    }
}

void GalaxyMapRenderer::drawStars(QPainter *painter)
{
    // Only debug star loading issues once per state change
    static bool hasLoggedEmptyStars = false;
    static int lastStarCount = -1;
    
    if (m_realStars.isEmpty() && !hasLoggedEmptyStars) {
        qDebug() << "drawStars: No stars loaded - checking data flow...";
        qDebug() << "  m_starSystems count:" << m_starSystems.size();
        qDebug() << "  Component size:" << width() << "x" << height();
        hasLoggedEmptyStars = true;
    } else if (!m_realStars.isEmpty() && (hasLoggedEmptyStars || lastStarCount != m_realStars.size())) {
        qDebug() << "✅ Stars loaded successfully:" << m_realStars.size() << "stars";
        hasLoggedEmptyStars = false;
        lastStarCount = m_realStars.size();
    }
    
    // Draw Elite Dangerous star systems (k-NN filtered in QML)
    int renderedCount = 0;
    
    // Draw only real Elite Dangerous star systems  
    for (const auto &star : m_realStars) {
        if (!isSystemVisible(star)) continue;
        
        // Check if star is roughly on screen (generous bounds)
        if (star.position.x() < -200 || star.position.x() > width() + 200 || 
            star.position.y() < -200 || star.position.y() > height() + 200) {
            continue;  // Skip systems that are way off-screen
        }
        
        // Check if this star is hovered
        bool isHovered = (reinterpret_cast<const StarSystem*>(m_hoveredSystem) == &star);
        
        painter->setBrush(QBrush(star.color));
        
        // Set pen with hover effect - different styles for different star types
        QPen pen;
        if (star.isBlackHole) {
            pen = QPen(isHovered ? QColor(255, 255, 0) : QColor(200, 100, 200), 
                      isHovered ? 3.0 / m_zoomLevel : 1.5 / m_zoomLevel);
        } else if (star.isNeutronStar) {
            pen = QPen(isHovered ? QColor(255, 255, 0) : QColor(255, 255, 255), 
                      isHovered ? 2.5 / m_zoomLevel : 1.0 / m_zoomLevel);
        } else if (star.isWhiteDwarf) {
            pen = QPen(isHovered ? QColor(255, 255, 0) : QColor(255, 255, 255), 
                      isHovered ? 2.0 / m_zoomLevel : 0.8 / m_zoomLevel);
        } else {
            pen = QPen(isHovered ? QColor(255, 255, 0) : QColor(255, 255, 255, 150), 
                      isHovered ? 2.0 / m_zoomLevel : 0.5 / m_zoomLevel);
        }
        painter->setPen(pen);
        
        // INVERSE scaling: bigger dots when zoomed out, smaller when zoomed in exponentially
        qreal baseSize = star.size * 0.8;  // Start with reasonable base size
        qreal inverseScale = qMax(0.1, qMin(3.0, 1.0 / qPow(m_zoomLevel, 0.7)));  // Exponential inverse scaling
        qreal drawSize = qMax(1.0, baseSize * inverseScale);  // Minimum 1 pixel for precision
        
        if (isHovered) {
            drawSize *= 1.2;  // Smaller hover increase to keep compact
        }
        
        // Simple improved star rendering with subtle gradient
        QColor coreColor = star.color;
        
        // Draw subtle outer glow for larger stars only
        if (drawSize > 4.0) {
            qreal glowSize = drawSize * 1.3;
            QRadialGradient glowGradient(star.position, glowSize * 0.5);
            glowGradient.setColorAt(0, QColor(coreColor.red(), coreColor.green(), coreColor.blue(), 60));
            glowGradient.setColorAt(0.7, QColor(coreColor.red(), coreColor.green(), coreColor.blue(), 20));
            glowGradient.setColorAt(1, QColor(0, 0, 0, 0));
            
            painter->setBrush(QBrush(glowGradient));
            painter->setPen(Qt::NoPen);
            QRectF glowRect(star.position.x() - glowSize/2, star.position.y() - glowSize/2, glowSize, glowSize);
            painter->drawEllipse(glowRect);
        }
        
        // Draw main star with simple gradient for better appearance
        QRadialGradient starGradient(star.position, drawSize * 0.5);
        if (isHovered) {
            starGradient.setColorAt(0, coreColor.lighter(160));
            starGradient.setColorAt(0.8, coreColor.lighter(120));
            starGradient.setColorAt(1, coreColor);
        } else {
            starGradient.setColorAt(0, coreColor.lighter(130));
            starGradient.setColorAt(0.8, coreColor);
            starGradient.setColorAt(1, coreColor.darker(110));
        }
        
        painter->setBrush(QBrush(starGradient));
        painter->setPen(pen);
        
        // Draw the star
        QRectF starRect(star.position.x() - drawSize/2, star.position.y() - drawSize/2, 
                       drawSize, drawSize);
        painter->drawEllipse(starRect);
        

        renderedCount++;  // Count rendered systems
        

        
        // Tooltip drawing removed from world space - now handled in screen space in drawUI()
    }
    
    // Only log star rendering if there were issues
    if (renderedCount == 0 && !m_realStars.isEmpty()) {
        qDebug() << "drawStars: Warning - 0 stars rendered out of" << m_realStars.size() << "total stars";
    }
    
    // Draw commander locations (current commander + all commanders in admin mode)
    drawCommanderLocations(painter);
}

void GalaxyMapRenderer::drawCommanderLocations(QPainter *painter)
{
    // Draw current commander location (green circle with crosshair)
    if (m_commanderPosition.x() != 0 || m_commanderPosition.y() != 0) {
        drawCommanderMarker(painter, m_commanderPosition, QColor(0, 255, 0), "CMDR", true);
    }
    
    // Draw all commander locations in admin mode (yellow circles)
    if (m_isAdminMode && m_showAllCommanders && !m_allCommanderLocations.isEmpty()) {
        for (const auto &locationVariant : m_allCommanderLocations) {
            QVariantMap location = locationVariant.toMap();
            
            QString commanderName = location.value("name", "Unknown").toString();
            qreal x = location.value("x", 0.0).toReal();
            qreal y = location.value("y", 0.0).toReal();
            qreal z = location.value("z", 0.0).toReal();
            
            // Convert 3D position to 2D screen position (using x,z as in Elite Dangerous)
            QPointF cmdrPos(x, z);
            
            // Only draw if not the current commander (to avoid duplicate markers)
            QString currentCmdr = location.value("currentCommander", "").toString();
            if (commanderName != currentCmdr && (x != 0 || z != 0)) {
                drawCommanderMarker(painter, cmdrPos, QColor(255, 255, 0), commanderName, false);
            }
        }
    }
}

void GalaxyMapRenderer::drawCommanderMarker(QPainter *painter, const QPointF &position, const QColor &color, const QString &name, bool isCurrent)
{
    // Calculate screen-space size (bigger when zoomed out, smaller when zoomed in)
    qreal baseSize = isCurrent ? 12.0 : 8.0;  // Current commander larger
    qreal markerSize = baseSize * qMax(0.5, qMin(2.0, 1.0 / qSqrt(m_zoomLevel)));
    
    // Draw outer circle
    painter->setPen(QPen(color, 2.0));
    painter->setBrush(QBrush(color.darker(150)));
    QRectF markerRect(position.x() - markerSize/2, position.y() - markerSize/2, markerSize, markerSize);
    painter->drawEllipse(markerRect);
    
    // Draw inner circle
    qreal innerSize = markerSize * 0.6;
    painter->setBrush(QBrush(color));
    QRectF innerRect(position.x() - innerSize/2, position.y() - innerSize/2, innerSize, innerSize);
    painter->drawEllipse(innerRect);
    
    // Draw crosshair for current commander
    if (isCurrent) {
        painter->setPen(QPen(color, 1.5));
        qreal crossSize = markerSize * 0.8;
        // Horizontal line
        painter->drawLine(position.x() - crossSize/2, position.y(), 
                         position.x() + crossSize/2, position.y());
        // Vertical line  
        painter->drawLine(position.x(), position.y() - crossSize/2,
                         position.x(), position.y() + crossSize/2);
    }
}

void GalaxyMapRenderer::drawUI(QPainter *painter)
{
    // Draw title with outline
    QFont titleFont("Arial", 18, QFont::Bold);
    painter->setFont(titleFont);
    QString title = "Elite Dangerous Galaxy Map";
    QRectF titleRect(0, 10, width(), 40);
    
    // Title outline
    painter->setPen(QPen(QColor(0, 0, 0), 3));
    painter->drawText(titleRect.adjusted(-1, -1, -1, -1), Qt::AlignCenter, title);
    painter->drawText(titleRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, title);
    
    // Title text
    painter->setPen(QPen(QColor(255, 127, 80)));  // Orange accent
    painter->drawText(titleRect, Qt::AlignCenter, title);
    
    painter->setFont(QFont("Arial", 12));
    painter->setPen(QPen(QColor(125, 211, 33)));
    
    // Count visible stars (only real Elite Dangerous systems)
    int visibleStars = 0;
    int renderedStars = 0;
    
    // Count visible and rendered stars
    for (const auto &star : m_realStars) {
        if (isSystemVisible(star)) {
            visibleStars++;
            // Check if star position is on screen
            if (!(star.position.x() < -200 || star.position.x() > width() + 200 || 
                  star.position.y() < -200 || star.position.y() > height() + 200)) {
                renderedStars++;
            }
        }
    }
    
    QString status = QString("Zoom: %1x | Showing: %2 of %3 systems | 🟦 Unclaimed  🟢 Your Claims  🟠 Others  🟡 POIs")
                     .arg(m_zoomLevel, 0, 'f', 2)
                     .arg(renderedStars)
                     .arg(m_realStars.size());
    
    // Status with outline
    painter->setPen(QPen(QColor(0, 0, 0), 2));
    painter->drawText(QRectF(0, 52, width(), 30), Qt::AlignCenter, status);
    
    painter->setPen(QPen(QColor(255, 255, 255)));
    painter->drawText(QRectF(0, 50, width(), 30), Qt::AlignCenter, status);
    
    // Draw tooltip in screen space (not affected by zoom/pan transforms)
    if (m_hoveredSystem && m_tooltipsEnabled) {
        StarSystem* hoveredStar = reinterpret_cast<StarSystem*>(m_hoveredSystem);
        
        // Convert world position to screen position
        QTransform transform;
        transform.translate(width() / 2.0, height() / 2.0);
        transform.translate(m_panOffset.x(), m_panOffset.y());
        transform.scale(m_zoomLevel, m_zoomLevel);
        transform.translate(-width() / 2.0, -height() / 2.0);
        
        QPointF screenPos = transform.map(hoveredStar->position);
        
        // Safety check: don't draw tooltip if star is at extreme position (coordinate overflow)
        if (qAbs(screenPos.x()) > 100000 || qAbs(screenPos.y()) > 100000) {
            return;
        }
        
        // Scale font size WITH zoom for better readability when zoomed in
        qreal baseFontSize = 12.0;
        // Cap font scaling at high zoom levels to prevent excessive tooltip sizes
        qreal zoomForFont = qMin(m_zoomLevel, 15.0);  // Cap effective zoom for font scaling
        qreal scaledFontSize = qMax(10.0, qMin(24.0, baseFontSize * qSqrt(zoomForFont)));
        QFont font("Arial", scaledFontSize);
        painter->setFont(font);
        
        QString displayText = hoveredStar->name;
        if (!hoveredStar->starClass.isEmpty()) {
            displayText += QString(" (%1)").arg(hoveredStar->starClass);
        }
        
        // Get text metrics for proper sizing
        QFontMetrics metrics(font);
        QRect textBounds = metrics.boundingRect(displayText);
        
        // Fixed UI elements in screen space
        qreal padding = 8.0;
        
        // Position tooltip directly above star (like v1.4incomplete.py)
        // anchor="s" means tooltip bottom is at the reference point
        qreal tooltipX = screenPos.x();
        qreal tooltipY = screenPos.y() - 15.0; // Offset above star, like cy-DOT_RADIUS-5
        
        // Adjust for text centering (since we want tooltip centered horizontally on star)
        QPointF tooltipPos = QPointF(tooltipX - textBounds.width() / 2.0, tooltipY);
        
        // Smart screen boundary detection in screen coordinates
        qreal rightEdge = tooltipPos.x() + textBounds.width() + padding * 2;
        qreal leftEdge = tooltipPos.x() - padding;
        qreal topEdge = tooltipPos.y() - textBounds.height() - padding;
        
        // Keep tooltip horizontally centered on star, but adjust if it goes off screen
        if (rightEdge > width()) {
            tooltipPos.setX(width() - textBounds.width() - padding * 2);
        }
        if (leftEdge < 0) {
            tooltipPos.setX(padding);
        }
        
        // If tooltip goes above screen, show it below the star instead
        if (topEdge < 0) {
            tooltipPos.setY(screenPos.y() + 25.0); // Show below star with some padding
        }
        
        // Draw background frame with fixed styling in screen space
        // Background should encompass the text with padding
        QRectF backgroundRect(tooltipPos.x() - padding, 
                              tooltipPos.y() - textBounds.height() - padding,
                              textBounds.width() + padding * 2, 
                              textBounds.height() + padding * 2);
        
        painter->setPen(QPen(QColor(255, 127, 80), 2.0));  // Orange border
        painter->setBrush(QBrush(QColor(0, 0, 0, 220)));  // Semi-transparent black
        painter->drawRoundedRect(backgroundRect, 4.0, 4.0);
        
        // Draw text
        painter->setPen(QPen(QColor(255, 255, 255)));  // White text
        painter->drawText(tooltipPos, displayText);
    }
}



void GalaxyMapRenderer::loadBackgroundImage()
{
    if (m_backgroundImage.isEmpty()) {
        m_backgroundLoaded = false;
        return;
    }
    
    // Try multiple paths to find the galaxy image
    QStringList possiblePaths = {
        QString(":/%1").arg(m_backgroundImage),  // Resource path
        QString("qrc:/%1").arg(m_backgroundImage),  // QRC resource
        m_backgroundImage,  // Direct path
        QString("%1/%2").arg(QDir::currentPath(), m_backgroundImage)  // Current dir
    };
    
    m_backgroundLoaded = false;
    for (const QString &path : possiblePaths) {
        qDebug() << "Trying to load galaxy background from:" << path;
        m_backgroundPixmap = QPixmap(path);
        if (!m_backgroundPixmap.isNull()) {
            m_backgroundLoaded = true;
            qDebug() << "✅ Galaxy background loaded successfully from:" << path;
            qDebug() << "   Image size:" << m_backgroundPixmap.size();
            break;
        }
    }
    
    if (!m_backgroundLoaded) {
        qDebug() << "❌ Failed to load galaxy background from all paths. Using procedural background.";
        for (const QString &path : possiblePaths) {
            qDebug() << "   Tried:" << path;
        }
    }
}

QColor GalaxyMapRenderer::getStarColor(const QString &type) const
{
    if (type == "center") return QColor(0, 255, 0);
    if (type == "corner") return QColor(0, 128, 255);
    if (type == "test") return QColor(255, 0, 0);
    if (type == "scattered") return QColor(255, 255, 255, 180);
    return QColor(255, 255, 255);
}

QPointF GalaxyMapRenderer::worldToScreen(const QPointF &worldPos) const
{
    QPointF screen = worldPos;
    screen = QPointF(screen.x() * m_zoomLevel, screen.y() * m_zoomLevel);
    screen += m_panOffset;
    screen += QPointF(width() / 2.0, height() / 2.0);
    return screen;
}

// Property setters
void GalaxyMapRenderer::setZoomLevel(qreal zoomLevel)
{
    if (qFuzzyCompare(m_zoomLevel, zoomLevel))
        return;
    
    m_zoomLevel = qBound(0.1, zoomLevel, 20.0);
    emit zoomLevelChanged();
    update();
}

void GalaxyMapRenderer::setPanOffset(const QPointF &panOffset)
{
    if (m_panOffset == panOffset)
        return;
    
    // Clamp pan offset to prevent extreme values that could cause instability
    qreal maxPan = 50000.0;
    QPointF clampedOffset(
        qBound(-maxPan, panOffset.x(), maxPan),
        qBound(-maxPan, panOffset.y(), maxPan)
    );
    
    m_panOffset = clampedOffset;
    emit panOffsetChanged();
    update();
}

void GalaxyMapRenderer::setBackgroundImage(const QString &backgroundImage)
{
    if (m_backgroundImage == backgroundImage)
        return;
    
    m_backgroundImage = backgroundImage;
    loadBackgroundImage();
    emit backgroundImageChanged();
    update();
}

void GalaxyMapRenderer::setStarSystems(const QVariantList &starSystems)
{
    if (m_starSystems == starSystems)
        return;
    
    // Only log significant changes in star system data
    static int lastStarSystemCount = -1;
    if (lastStarSystemCount != starSystems.size()) {
        qDebug() << "Galaxy Renderer: Star systems updated to" << starSystems.size() << "systems";
        lastStarSystemCount = starSystems.size();
    }
    
    m_starSystems = starSystems;
    loadRealStarSystems();  // Convert and load the star data
    emit starSystemsChanged();
    update();
}



void GalaxyMapRenderer::setShowMainSequence(bool show)
{
    if (m_showMainSequence == show)
        return;
    
    m_showMainSequence = show;
    emit showMainSequenceChanged();
    update();
}

void GalaxyMapRenderer::setShowNeutronStars(bool show)
{
    if (m_showNeutronStars == show)
        return;
    
    m_showNeutronStars = show;
    emit showNeutronStarsChanged();
    update();
}

void GalaxyMapRenderer::setShowWhiteDwarfs(bool show)
{
    if (m_showWhiteDwarfs == show)
        return;
    
    m_showWhiteDwarfs = show;
    emit showWhiteDwarfsChanged();
    update();
}

void GalaxyMapRenderer::setShowBlackHoles(bool show)
{
    if (m_showBlackHoles == show)
        return;
    
    m_showBlackHoles = show;
    emit showBlackHolesChanged();
    update();
}

void GalaxyMapRenderer::setTooltipsEnabled(bool enabled)
{
    if (m_tooltipsEnabled == enabled)
        return;
    
    m_tooltipsEnabled = enabled;
    emit tooltipsEnabledChanged();
    update();
}

void GalaxyMapRenderer::setCommanderPosition(const QPointF &position)
{
    if (m_commanderPosition == position)
        return;
    
    m_commanderPosition = position;
    emit commanderPositionChanged();
    
    // Reload star positions when commander position changes
    if (!m_starSystems.isEmpty()) {
        loadRealStarSystems();
    }
}

void GalaxyMapRenderer::setAllCommanderLocations(const QVariantList &locations)
{
    if (m_allCommanderLocations == locations)
        return;
    
    m_allCommanderLocations = locations;
    emit allCommanderLocationsChanged();
    update();  // Redraw to show commander locations
}

void GalaxyMapRenderer::setShowAllCommanders(bool show)
{
    if (m_showAllCommanders == show)
        return;
    
    m_showAllCommanders = show;
    emit showAllCommandersChanged();
    update();  // Redraw to show/hide commander locations
}

void GalaxyMapRenderer::setIsAdminMode(bool isAdmin)
{
    if (m_isAdminMode == isAdmin)
        return;
    
    m_isAdminMode = isAdmin;
    emit isAdminModeChanged();
    update();  // Redraw with admin features
}

void GalaxyMapRenderer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        // Find system at click position
        StarSystem* system = findSystemAtPosition(event->position());
        if (system) {
            qDebug() << "Right-clicked on system:" << system->name;
            emit systemRightClicked(system->name, system->x, system->y, system->z);
        }
    }
    QQuickPaintedItem::mousePressEvent(event);
}

void GalaxyMapRenderer::mouseMoveEvent(QMouseEvent *event)
{
    QQuickPaintedItem::mouseMoveEvent(event);
}

void GalaxyMapRenderer::hoverMoveEvent(QHoverEvent *event)
{
    // Store mouse position for tooltip placement
    m_lastMousePosition = event->position();
    
    // Find system at hover position
    StarSystem* system = findSystemAtPosition(event->position());
    if (system != reinterpret_cast<StarSystem*>(m_hoveredSystem)) {
        m_hoveredSystem = reinterpret_cast<QObject*>(system);

        emit hoveredSystemChanged();
        update();  // Redraw to show hover effect
    }
    QQuickPaintedItem::hoverMoveEvent(event);
}

GalaxyMapRenderer::StarSystem* GalaxyMapRenderer::findSystemAtPosition(const QPointF &pos)
{
    // Add stability check for high zoom levels
    if (m_zoomLevel < 0.001 || m_zoomLevel > 50.0) {
        return nullptr;
    }
    
    // Transform mouse position to the same coordinate space as stored star positions
    // We need to reverse the transformation applied in paint()
    QTransform transform;
    transform.translate(width() / 2.0, height() / 2.0);
    transform.translate(m_panOffset.x(), m_panOffset.y());
    transform.scale(m_zoomLevel, m_zoomLevel);
    transform.translate(-width() / 2.0, -height() / 2.0);
    
    // Get the inverse transform to convert mouse coords to star coord space
    QTransform inverseTransform = transform.inverted();
    if (!inverseTransform.isInvertible()) {
        return nullptr;  // Safety check for high zoom levels
    }
    
    QPointF transformedPos = inverseTransform.map(pos);
    
    // Bounds check on transformed position to prevent overflow
    if (qAbs(transformedPos.x()) > 1000000 || qAbs(transformedPos.y()) > 1000000) {
        return nullptr;
    }
    
    // Check only real Elite Dangerous stars  
    for (auto &star : m_realStars) {
        if (!isSystemVisible(star)) continue;
        
        QPointF starPos = star.position;
        qreal distance = QPointF(transformedPos - starPos).manhattanLength();
        
        // Adjusted hit radius for smaller visual dots - keep reasonable click area
        // Make hit radius larger than visual dot for easier clicking
        qreal baseHitRadius = qMax(8.0, star.size * 1.5);  // Larger base than visual size
        qreal hitRadius = baseHitRadius / qMax(1.0, m_zoomLevel);  // Prevent division by near-zero
        
        // At high zoom, ensure minimum clickable area
        if (m_zoomLevel > 5.0) {
            hitRadius = qMax(hitRadius, 12.0 / m_zoomLevel);
        }
        
        // At very high zoom, increase hit radius for easier interaction
        if (m_zoomLevel > 15.0) {
            hitRadius = qMax(hitRadius, 20.0 / m_zoomLevel);
        }
        
        if (distance <= hitRadius) {
            return &star;
        }
    }
    
    return nullptr;
}

bool GalaxyMapRenderer::isSystemVisible(const StarSystem &system) const
{
    // Only filter by actual star types for Elite Dangerous systems
    if (system.isMainSequence && !m_showMainSequence) return false;
    if (system.isNeutronStar && !m_showNeutronStars) return false;
    if (system.isWhiteDwarf && !m_showWhiteDwarfs) return false;
    if (system.isBlackHole && !m_showBlackHoles) return false;
    
    return true;
}

void GalaxyMapRenderer::loadRealStarSystems()
{
    m_realStars.clear();
    
    if (m_starSystems.isEmpty()) {
        qDebug() << "loadRealStarSystems: No star systems data available, count=" << m_starSystems.size();
        return;
    }
    
    // Wait for the component to be properly sized
    if (width() <= 0 || height() <= 0) {
        qDebug() << "loadRealStarSystems: Component not properly sized yet, width=" << width() << "height=" << height() << "- will retry when sized";
        return;
    }
    
    qDebug() << "loadRealStarSystems: Processing" << m_starSystems.size() << "systems with component size" << width() << "x" << height();
    
    // Debug first few systems to see what data looks like
    for (int i = 0; i < qMin(3, m_starSystems.size()); i++) {
        QVariantMap systemData = m_starSystems[i].toMap();
        qDebug() << "  Sample system" << i << ":" << systemData["name"].toString() << 
                    "coords:" << systemData["x"].toDouble() << systemData["y"].toDouble() << systemData["z"].toDouble();
    }
    

    
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    
    // Convert from QVariantList to internal format
    for (const auto &variant : m_starSystems) {
        QVariantMap systemData = variant.toMap();
        
        StarSystem star;
        star.name = systemData.value("name", "Unknown").toString();
        star.x = systemData.value("x", 0.0).toReal();
        star.y = systemData.value("y", 0.0).toReal();
        star.z = systemData.value("z", 0.0).toReal();
        
        // Get category instead of star class since that's what we have
        QString category = systemData.value("category", "").toString();
        star.starClass = category;  // Use category as star class for now
        
        // Convert Elite Dangerous galactic coordinates to screen coordinates
        // Use EXACT same transformation as Python v1.4incomplete.py
        const qreal LY_PER_PIXEL = 40.0;
        const qreal ORIG_OFF_X = 1124.0;
        const qreal ORIG_OFF_Y = 1749.0;
        
        // Step 1: Transform to pixel coordinates (same as Python)
        qreal px = ORIG_OFF_X + star.x / LY_PER_PIXEL;
        qreal py = ORIG_OFF_Y - star.z / LY_PER_PIXEL;  // CRITICAL: Minus for Z coordinate!
        
        // Step 2: Python scaling logic (EXACTLY like Python v1.4incomplete.py):
        // Python: base_full = 2250x2250, base_med = 800x800, zoom = 1.0 default
        // Python: resized = base_med * zoom, scale = resized.width / base_full.width
        
        qreal base_full_width = 2250.0;
        qreal base_full_height = 2250.0;
        qreal base_med_width = 800.0;   // Python resizes original to 800x800
        qreal zoom = 1.5;  // Default zoom to make galaxy bigger (Python default is 1.0 but that's too small)
        
        // Calculate resized dimensions (Python: base_med.width * zoom)
        qreal resized_width = base_med_width * zoom;
        qreal resized_height = base_med_width * zoom;  // Square image
        
        // Calculate scale factor (Python: scale = resized.width / base_full.width)
        qreal scale = resized_width / base_full_width;
        
        qreal canvas_width = width();
        qreal canvas_height = height();
        
        // Use the calculated scale for positioning
        qreal image_width = resized_width;
        qreal image_height = resized_height;
        
        // Center the scaled image in the window (Python: x0, y0 = (cw-im_w)/2, (ch-im_h)/2)
        qreal x0 = (canvas_width - image_width) / 2.0;
        qreal y0 = (canvas_height - image_height) / 2.0;
        
        // Apply final transformation (Python: cx, cy = x0 + px*scale, y0 + py*scale)
        star.position = QPointF(
            x0 + px * scale,
            y0 + py * scale
        );
        
        // Skip systems with extreme coordinates
        if (qAbs(star.position.x()) > width() * 5 || qAbs(star.position.y()) > height() * 5) {
            continue;
        }
        
        // Determine star type and properties based on category
        star.type = "real";
        
        // Color by CLAIM STATUS, not category (much more useful!)
        bool claimed = systemData.value("claimed", false).toBool();
        bool isPOI = !systemData.value("poi", "").toString().isEmpty();
        QString claimedBy = systemData.value("claimedBy", "").toString();
        
        // Color coding for Elite Dangerous galaxy map
        if (isPOI) {
            // POI systems - bright gold
            star.color = QColor(255, 215, 0);  // Gold
            star.size = 8;
        } else if (claimed) {
            QString currentCommander = "Regza"; // Get from controller
            if (claimedBy == currentCommander) {
                // Your claims - bright green
                star.color = QColor(50, 255, 50);  // Bright green
                star.size = 6;
            } else {
                // Other claims - orange
                star.color = QColor(255, 140, 0);  // Orange
                star.size = 6;
            }
        } else {
            // Unclaimed systems - cyan/teal (most common)
            star.color = QColor(0, 180, 180);  // Cyan/teal - easier to distinguish
            star.size = 5;
        }
        
        // Override with special categories if important
        if (category.contains("Black hole", Qt::CaseInsensitive)) {
            star.color = QColor(128, 0, 128);  // Purple
            star.size = 10;
        } else if (category.contains("Neutron", Qt::CaseInsensitive)) {
            star.color = QColor(200, 200, 255);  // Bright blue-white
            star.size = 8;
        }
        
        // Set star classification for display
        star.isMainSequence = true;
        star.isBlackHole = false;
        star.isNeutronStar = false;
        star.isWhiteDwarf = false;
        
                m_realStars.append(star);
    }
    
    qDebug() << "loadRealStarSystems: Successfully loaded" << m_realStars.size() << "stars from" << m_starSystems.size() << "systems";
    
    // Debug a few stars to see their positions
    for (int i = 0; i < qMin(3, m_realStars.size()); i++) {
        const auto& star = m_realStars[i];
        qDebug() << "  Star" << i << star.name << "at position" << star.position << "color" << star.color << "size" << star.size;
    }
    
    update();
}

void GalaxyMapRenderer::loadSampleEliteStars()
{
    m_realStars.clear();
    
    // Wait for the component to be properly sized
    if (width() <= 0 || height() <= 0) {
        return;
    }
    
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    
    // Create some realistic Elite Dangerous systems
    QList<QPair<QString, QString>> eliteSystems = {
        {"Sol", "G2V"},
        {"Alpha Centauri", "G2V"},
        {"Wolf 359", "M6V"},
        {"Lalande 21185", "M2V"},
        {"Sirius", "A1V"},
        {"Procyon", "F5IV"},
        {"Epsilon Eridani", "K2V"},
        {"61 Cygni", "K5V"},
        {"Epsilon Indi", "K5V"},
        {"Tau Ceti", "G8V"},
        {"Vega", "A0V"},
        {"Altair", "A7V"},
        {"Fomalhaut", "A3V"},
        {"Rigel", "B8Ia"},
        {"Betelgeuse", "M1Ia"},
        {"Antares", "M1Ib"},
        {"Polaris", "F7Ib"},
        {"Capella", "G5III"},
        {"Aldebaran", "K5III"},
        {"Arcturus", "K1III"},
        {"Spica", "B1V"},
        {"Regulus", "B7V"},
        {"Deneb", "A2Ia"},
        {"Canopus", "F0II"},
        {"Achernar", "B6Ve"},
        // Add some neutron stars and white dwarfs
        {"PSR B1919+21", "NS"},
        {"PSR J1748-2446ad", "NS"},
        {"Sirius B", "DA2"},
        {"Procyon B", "DQZ"},
        {"40 Eridani B", "DA4"},
        // Add some black holes
        {"Sagittarius A*", "BH"},
        {"Cygnus X-1", "BH"},
        {"V404 Cygni", "BH"}
    };
    
    // Generate stars in a galaxy-like distribution
    for (int i = 0; i < eliteSystems.size(); ++i) {
        StarSystem star;
        
        auto systemData = eliteSystems[i];
        star.name = systemData.first;
        star.starClass = systemData.second;
        
        // Generate realistic 3D coordinates (in light years)
        qreal angle = QRandomGenerator::global()->bounded(360) * M_PI / 180.0;
        qreal distance = 25.0 + QRandomGenerator::global()->bounded(25000); // 25-25000 ly from center
        
        star.x = distance * cos(angle);
        star.y = (QRandomGenerator::global()->bounded(200) - 100); // Small Y variation
        star.z = distance * sin(angle);
        
        // Convert to screen coordinates (scaled down)
        qreal scale = 0.01; // 1 ly = 0.01 pixels
        star.position = QPointF(
            centerX + star.x * scale,
            centerY + star.z * scale
        );
        
        star.type = "real";
        
        // Determine star type and properties
        star.isMainSequence = star.starClass.startsWith("M") || star.starClass.startsWith("K") || 
                             star.starClass.startsWith("G") || star.starClass.startsWith("F") ||
                             star.starClass.startsWith("A") || star.starClass.startsWith("B") ||
                             star.starClass.startsWith("O");
        star.isNeutronStar = star.starClass.contains("NS") || star.starClass.contains("Neutron");
        star.isWhiteDwarf = star.starClass.startsWith("D");
        star.isBlackHole = star.starClass.contains("BH") || star.starClass.contains("Black Hole");
        
        // Set color and size based on star type
        if (star.isBlackHole) {
            star.color = QColor(80, 0, 80);  // Dark purple
            star.size = 16;
        } else if (star.isNeutronStar) {
            star.color = QColor(200, 200, 255);  // Bright blue-white
            star.size = 12;
        } else if (star.isWhiteDwarf) {
            star.color = QColor(255, 255, 255);  // White
            star.size = 8;
        } else if (star.isMainSequence) {
            // Realistic spectral class colors
            if (star.starClass.startsWith("O")) {
                star.color = QColor(155, 176, 255);  // Blue
                star.size = 12;
            } else if (star.starClass.startsWith("B")) {
                star.color = QColor(170, 191, 255);  // Blue-white
                star.size = 10;
            } else if (star.starClass.startsWith("A")) {
                star.color = QColor(202, 215, 255);  // White
                star.size = 8;
            } else if (star.starClass.startsWith("F")) {
                star.color = QColor(248, 247, 255);  // Yellow-white
                star.size = 7;
            } else if (star.starClass.startsWith("G")) {
                star.color = QColor(255, 244, 234);  // Yellow (like our Sun)
                star.size = 6;
            } else if (star.starClass.startsWith("K")) {
                star.color = QColor(255, 210, 161);  // Orange
                star.size = 5;
            } else if (star.starClass.startsWith("M")) {
                star.color = QColor(255, 204, 111);  // Red
                star.size = 4;
            } else {
                star.color = QColor(255, 255, 255);  // Default white
                star.size = 6;
            }
        } else {
            star.color = QColor(255, 255, 255);  // Default white
            star.size = 6;
        }
        
        m_realStars.append(star);
    }
    
    // Add Sagittarius A* at the center
    StarSystem sagA;
    sagA.name = "Sagittarius A*";
    sagA.starClass = "SMBH";
    sagA.x = 0;
    sagA.y = 0;
    sagA.z = 0;
    sagA.position = QPointF(centerX, centerY);
    sagA.type = "real";
    sagA.isBlackHole = true;
    sagA.isMainSequence = false;
    sagA.isNeutronStar = false;
    sagA.isWhiteDwarf = false;
    sagA.color = QColor(120, 0, 120);  // Bright purple for supermassive black hole
    sagA.size = 20;  // Largest
    
    m_realStars.prepend(sagA);  // Add at beginning so it's drawn first
    
    qDebug() << "Generated" << m_realStars.size() << "sample Elite Dangerous star systems";
}

void GalaxyMapRenderer::repositionStarsAfterResize()
{
    if (m_realStars.isEmpty() || width() <= 0 || height() <= 0) {
        return;
    }
    
    qreal centerX = width() / 2.0;
    qreal centerY = height() / 2.0;
    qreal scale = 0.01; // Same scale as loadSampleEliteStars
    
    // Reposition all stars based on their 3D coordinates
    for (auto &star : m_realStars) {
        star.position = QPointF(
            centerX + star.x * scale,
            centerY + star.z * scale
        );
    }
    
    qDebug() << "Repositioned" << m_realStars.size() << "stars after resize";
} 