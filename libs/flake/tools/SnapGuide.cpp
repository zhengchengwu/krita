/* This file is part of the KDE project
 * Copyright (C) 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "SnapGuide.h"
#include <KoPathShape.h>
#include <KoPathPoint.h>
#include <KoViewConverter.h>
#include <KoCanvasBase.h>
#include <KoShapeManager.h>

#include <QtGui/QPainter>

#include <math.h>


SnapGuide::SnapGuide( KoCanvasBase * canvas )
    : m_canvas(canvas), m_extraShape(0), m_currentStrategy(0)
    , m_active(true), m_snapDistance(10)
{
    m_strategies.append( new NodeSnapStrategy() );
    m_strategies.append( new OrthogonalSnapStrategy() );
}

SnapGuide::~SnapGuide()
{
}

void SnapGuide::setExtraShape( KoShape * shape )
{
    m_extraShape = shape;
}

KoShape * SnapGuide::extraShape() const
{
    return m_extraShape;
}

void SnapGuide::enableSnapStrategies( int strategies )
{
    m_usedStrategies = strategies;
}

int SnapGuide::enabledSnapStrategies() const
{
    return m_usedStrategies;
}

void SnapGuide::enableSnapping( bool on )
{
    m_active = on;
}

bool SnapGuide::isSnapping() const
{
    return m_active;
}

void SnapGuide::setSnapDistance( int distance )
{
    m_snapDistance = qAbs( distance );
}

int SnapGuide::snapDistance() const
{
    return m_snapDistance;
}

QPointF SnapGuide::snap( const QPointF &mousePosition )
{
    if( ! m_active )
        return mousePosition;

    SnapProxy proxy( this );

    double minDistance = HUGE_VAL;

    m_currentStrategy = 0;

    double maxSnapDistance = m_canvas->viewConverter()->viewToDocument( QSizeF( m_snapDistance, m_snapDistance ) ).width();
    foreach( SnapStrategy * strategy, m_strategies )
    {
        if( m_usedStrategies & strategy->type() )
        {
            if( ! strategy->snapToPoints( mousePosition, &proxy, maxSnapDistance ) )
                continue;

            QPointF snapCandidate = strategy->snappedPosition();
            double distance = SnapStrategy::fastDistance( snapCandidate, mousePosition );
            if( distance < minDistance )
            {
                m_currentStrategy = strategy;
                minDistance = distance;
            }
        }
    }

    if( ! m_currentStrategy )
        return mousePosition;

    return m_currentStrategy->snappedPosition();
}

QRectF SnapGuide::boundingRect()
{
    QRectF rect;

    if( m_currentStrategy )
        rect = m_currentStrategy->decoration().boundingRect();

    return rect.adjusted( -2, -2, 2, 2 );
}

void SnapGuide::paint( QPainter &painter, const KoViewConverter &converter )
{
    if( ! m_currentStrategy )
        return;

    QPen pen( Qt::red );
    pen.setStyle( Qt::DotLine );
    painter.setPen( pen );
    painter.setBrush( Qt::NoBrush );
    painter.drawPath( m_currentStrategy->decoration() );
}

KoCanvasBase * SnapGuide::canvas() const
{
    return m_canvas;
}

/////////////////////////////////////////////////////////
// snap proxy
/////////////////////////////////////////////////////////

SnapProxy::SnapProxy( SnapGuide * snapGuide )
    : m_snapGuide(snapGuide)
{
}

QList<QPointF> SnapProxy::pointsInRect( const QRectF &rect )
{
    QList<QPointF> points;
    QList<KoShape*> shapes = shapesInRect( rect );
    foreach( KoShape * shape, shapes )
    {
        foreach( QPointF point, pointsFromShape( shape ) )
        {
            if( rect.contains( point ) )
                points.append( point );
        }
    }

    return points;
}

QList<KoShape*> SnapProxy::shapesInRect( const QRectF &rect )
{
    QList<KoShape*> shapes = m_snapGuide->canvas()->shapeManager()->shapesAt( rect );

    if( m_snapGuide->extraShape() )
    {
        QRectF bound = m_snapGuide->extraShape()->boundingRect();
        if( rect.intersects( bound ) || rect.contains( bound ) )
            shapes.append( m_snapGuide->extraShape() );
    }
    return shapes;
}

QList<QPointF> SnapProxy::pointsFromShape( KoShape * shape )
{
    QList<QPointF> pathPoints;

    KoPathShape * path = dynamic_cast<KoPathShape*>( shape );
    if( ! path )
        return pathPoints;

    QMatrix m = path->absoluteTransformation(0);

    int subpathCount = path->subpathCount();
    for( int subpathIndex = 0; subpathIndex < subpathCount; ++subpathIndex )
    {
        int pointCount = path->pointCountSubpath( subpathIndex );
        for( int pointIndex = 0; pointIndex < pointCount; ++pointIndex )
        {
            KoPathPoint * p = path->pointByIndex( KoPathPointIndex( subpathIndex, pointIndex )  );
            if( ! p )
                continue;

            pathPoints.append( m.map( p->point() ) );
        }
    }

    if( shape == m_snapGuide->extraShape() )
        pathPoints.removeLast();

    return pathPoints;
}

QList<KoShape*> SnapProxy::shapes()
{
    QList<KoShape*> shapes = m_snapGuide->canvas()->shapeManager()->shapes();
    if( m_snapGuide->extraShape() )
        shapes.append( m_snapGuide->extraShape() );
    return shapes;
}

/////////////////////////////////////////////////////////
// snap strategies
/////////////////////////////////////////////////////////

SnapStrategy::SnapStrategy( SnapGuide::SnapType type )
    : m_snapType(type)
{
}

QPainterPath SnapStrategy::decoration() const
{
    return m_decoration;
}

void SnapStrategy::setDecoration( const QPainterPath &decoration )
{
    m_decoration = decoration;
}

QPointF SnapStrategy::snappedPosition() const
{
    return m_snappedPosition;
}

void SnapStrategy::setSnappedPosition( const QPointF &position )
{
    m_snappedPosition= position;
}

SnapGuide::SnapType SnapStrategy::type() const
{
    return m_snapType;
}

double SnapStrategy::fastDistance( const QPointF &p1, const QPointF &p2 )
{
    double dx = p1.x()-p2.x();
    double dy = p1.y()-p2.y();
    return dx*dx + dy*dy;
}

OrthogonalSnapStrategy::OrthogonalSnapStrategy()
    : SnapStrategy( SnapGuide::Orthogonal )
{
}

bool OrthogonalSnapStrategy::snapToPoints( const QPointF &mousePosition, SnapProxy * proxy, double maxSnapDistance )
{
    QPointF horzSnap, vertSnap;
    double minVertDist = HUGE_VAL;
    double minHorzDist = HUGE_VAL;

    QList<KoShape*> shapes = proxy->shapes();
    foreach( KoShape * shape, shapes )
    {
        QList<QPointF> points = proxy->pointsFromShape( shape );
        foreach( QPointF point, points )
        {
            double dx = fabs( point.x() - mousePosition.x() );
            if( dx < minHorzDist && dx < maxSnapDistance )
            {
                minHorzDist = dx;
                horzSnap = point;
            }
            double dy = fabs( point.y() - mousePosition.y() );
            if( dy < minVertDist && dy < maxSnapDistance )
            {
                minVertDist = dy;
                vertSnap = point;
            }
        }
    }

    QPointF snappedPoint = mousePosition;

    if( minHorzDist < HUGE_VAL )
        snappedPoint.setX( horzSnap.x() );
    if( minVertDist < HUGE_VAL )
        snappedPoint.setY( vertSnap.y() );

    QPainterPath decoration;

    if( minHorzDist < HUGE_VAL )
    {
        decoration.moveTo( horzSnap );
        decoration.lineTo( snappedPoint );
    }

    if( minVertDist < HUGE_VAL )
    {
        decoration.moveTo( vertSnap );
        decoration.lineTo( snappedPoint );
    }

    setDecoration( decoration );
    setSnappedPosition( snappedPoint );

    return (minHorzDist < HUGE_VAL || minVertDist < HUGE_VAL);
}

NodeSnapStrategy::NodeSnapStrategy()
    : SnapStrategy( SnapGuide::Node )
{
}

bool NodeSnapStrategy::snapToPoints( const QPointF &mousePosition, SnapProxy * proxy, double maxSnapDistance )
{
    double maxDistance = maxSnapDistance*maxSnapDistance;
    double minDistance = HUGE_VAL;

    QRectF rect( -maxSnapDistance, -maxSnapDistance, maxSnapDistance, maxSnapDistance );
    rect.moveCenter( mousePosition );
    QList<QPointF> points = proxy->pointsInRect( rect );

    QPointF snappedPoint = mousePosition;

    foreach( QPointF point, points )
    {
        QPointF diffVec = mousePosition-point;
        double distance = diffVec.x()*diffVec.x() + diffVec.y()*diffVec.y();
        if( distance < maxDistance && distance < minDistance )
        {
            snappedPoint = point;
            minDistance = distance;
        }
    }

    QPainterPath decoration;

    if( minDistance < HUGE_VAL )
    {
        QRectF ellipse( -10, -10, 10, 10 );
        ellipse.moveCenter( snappedPoint );
        decoration.addEllipse( ellipse );
    }

    setDecoration( decoration );
    setSnappedPosition( snappedPoint );

    return (minDistance < HUGE_VAL);
}
