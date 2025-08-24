#ifndef GALAXYMAPRENDERER_H
#define GALAXYMAPRENDERER_H

#include <QQuickPaintedItem>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QColor>
#include <QVariantList>

class GalaxyMapRenderer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(qreal zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    Q_PROPERTY(QPointF panOffset READ panOffset WRITE setPanOffset NOTIFY panOffsetChanged)
    Q_PROPERTY(QString backgroundImage READ backgroundImage WRITE setBackgroundImage NOTIFY backgroundImageChanged)
    Q_PROPERTY(QVariantList starSystems READ starSystems WRITE setStarSystems NOTIFY starSystemsChanged)
    Q_PROPERTY(bool showMainSequence READ showMainSequence WRITE setShowMainSequence NOTIFY showMainSequenceChanged)
    Q_PROPERTY(bool showNeutronStars READ showNeutronStars WRITE setShowNeutronStars NOTIFY showNeutronStarsChanged)
    Q_PROPERTY(bool showWhiteDwarfs READ showWhiteDwarfs WRITE setShowWhiteDwarfs NOTIFY showWhiteDwarfsChanged)
    Q_PROPERTY(bool showBlackHoles READ showBlackHoles WRITE setShowBlackHoles NOTIFY showBlackHolesChanged)
    Q_PROPERTY(bool tooltipsEnabled READ tooltipsEnabled WRITE setTooltipsEnabled NOTIFY tooltipsEnabledChanged)
    Q_PROPERTY(QObject* hoveredSystem READ hoveredSystem NOTIFY hoveredSystemChanged)
    Q_PROPERTY(QPointF commanderPosition READ commanderPosition WRITE setCommanderPosition NOTIFY commanderPositionChanged)
    Q_PROPERTY(QVariantList allCommanderLocations READ allCommanderLocations WRITE setAllCommanderLocations NOTIFY allCommanderLocationsChanged)
    Q_PROPERTY(bool showAllCommanders READ showAllCommanders WRITE setShowAllCommanders NOTIFY showAllCommandersChanged)
    Q_PROPERTY(bool isAdminMode READ isAdminMode WRITE setIsAdminMode NOTIFY isAdminModeChanged)

public:
    explicit GalaxyMapRenderer(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;

    // Property getters
    qreal zoomLevel() const { return m_zoomLevel; }
    QPointF panOffset() const { return m_panOffset; }
    QString backgroundImage() const { return m_backgroundImage; }
    QVariantList starSystems() const { return m_starSystems; }
    bool showMainSequence() const { return m_showMainSequence; }
    bool showNeutronStars() const { return m_showNeutronStars; }
    bool showWhiteDwarfs() const { return m_showWhiteDwarfs; }
    bool showBlackHoles() const { return m_showBlackHoles; }
    bool tooltipsEnabled() const { return m_tooltipsEnabled; }
    QObject* hoveredSystem() const { return m_hoveredSystem; }
    QPointF commanderPosition() const { return m_commanderPosition; }
    QVariantList allCommanderLocations() const { return m_allCommanderLocations; }
    bool showAllCommanders() const { return m_showAllCommanders; }
    bool isAdminMode() const { return m_isAdminMode; }

    // Property setters
    void setZoomLevel(qreal zoomLevel);
    void setPanOffset(const QPointF &panOffset);
    void setBackgroundImage(const QString &backgroundImage);
    void setStarSystems(const QVariantList &starSystems);
    void setShowMainSequence(bool show);
    void setShowNeutronStars(bool show);
    void setShowWhiteDwarfs(bool show);
    void setShowBlackHoles(bool show);
    void setTooltipsEnabled(bool enabled);
    void setCommanderPosition(const QPointF &position);
    void setAllCommanderLocations(const QVariantList &locations);
    void setShowAllCommanders(bool show);
    void setIsAdminMode(bool isAdmin);

signals:
    void zoomLevelChanged();
    void panOffsetChanged();
    void backgroundImageChanged();
    void starSystemsChanged();
    void showMainSequenceChanged();
    void showNeutronStarsChanged();
    void showWhiteDwarfsChanged();
    void showBlackHolesChanged();
    void tooltipsEnabledChanged();
    void hoveredSystemChanged();
    void commanderPositionChanged();
    void allCommanderLocationsChanged();
    void showAllCommandersChanged();
    void isAdminModeChanged();
    void systemRightClicked(const QString &systemName, qreal x, qreal y, qreal z);

private:
    struct StarSystem {
        QPointF position;
        QString type;
        QString name;
        QColor color;
        qreal size;
        qreal x, y, z;  // 3D coordinates
        QString starClass;
        bool isMainSequence;
        bool isNeutronStar;
        bool isWhiteDwarf;
        bool isBlackHole;
    };

    void loadBackgroundImage();
    void loadRealStarSystems();
    void loadSampleEliteStars();
    void repositionStarsAfterResize();
    QPointF worldToScreen(const QPointF &worldPos) const;
    QColor getStarColor(const QString &type) const;
    void drawBackground(QPainter *painter);
    void drawStars(QPainter *painter);
    void drawUI(QPainter *painter);
    void drawCommanderLocations(QPainter *painter);
    void drawCommanderMarker(QPainter *painter, const QPointF &position, const QColor &color, const QString &name, bool isCurrent);
    StarSystem* findSystemAtPosition(const QPointF &pos);
    bool isSystemVisible(const StarSystem &system) const;

    qreal m_zoomLevel;
    QPointF m_panOffset;
    QString m_backgroundImage;
    QPixmap m_backgroundPixmap;
    QVariantList m_starSystems;
    QList<StarSystem> m_realStars;
    
    bool m_showMainSequence;
    bool m_showNeutronStars;
    bool m_showWhiteDwarfs;
    bool m_showBlackHoles;
    bool m_tooltipsEnabled;
    bool m_backgroundLoaded;
    
    QObject* m_hoveredSystem;
    QPointF m_commanderPosition;
    QVariantList m_allCommanderLocations;
    bool m_showAllCommanders;
    bool m_isAdminMode;
    
    // Add mouse position tracking
    QPointF m_lastMousePosition;
};

#endif // GALAXYMAPRENDERER_H 