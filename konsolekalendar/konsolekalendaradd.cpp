/*******************************************************************************
 * konsolekalendaradd.cpp                                                      *
 *                                                                             *
 * KonsoleKalendar is a command line interface to KDE calendars                *
 * Copyright (C) 2002-2004  Tuukka Pasanen <illuusio@mailcity.com>             *
 * Copyright (C) 2003-2005  Allen Winter <winter@kde.org>                      *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        *
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation; either version 2 of the License, or           *
 * (at your option) any later version.                                         *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. *
 *                                                                             *
 * As a special exception, permission is given to link this program            *
 * with any edition of Qt, and distribute the resulting executable,            *
 * without including the source code for Qt in the source distribution.        *
 *                                                                             *
 ******************************************************************************/
/**
 * @file konsolekalendaradd.cpp
 * Provides the KonsoleKalendarAdd class definition.
 * @author Tuukka Pasanen
 * @author Allen Winter
 */
#include "konsolekalendaradd.h"

#include <CalendarSupport/KCalPrefs>

#include "konsolekalendar_debug.h"

#include <KLocalizedString>

#include <KCalCore/Event>
#include <KCalCore/FileStorage>

#include <Akonadi/Calendar/IncidenceChanger>
#include <AkonadiCore/Collection>

#include <QDateTime>
#include <QObject>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimeZone>

#include <stdlib.h>
#include <iostream>
#include <QStandardPaths>

using namespace KCalCore;
using namespace std;

KonsoleKalendarAdd::KonsoleKalendarAdd(KonsoleKalendarVariables *vars)
{
    m_variables = vars;
}

KonsoleKalendarAdd::~KonsoleKalendarAdd()
{
}

/**
 * Adds event to Calendar
 */

bool KonsoleKalendarAdd::addEvent()
{
    bool status = true;

    qCDebug(KONSOLEKALENDAR_LOG) << "konsolekalendaradd.cpp::addEvent()";

    if (m_variables->isDryRun()) {
        cout << i18n("Insert Event <Dry Run>:").toLocal8Bit().data()
             << endl;
        printSpecs();
    } else {
        if (m_variables->isVerbose()) {
            cout << i18n("Insert Event <Verbose>:").toLocal8Bit().data()
                 << endl;
            printSpecs();
        }

        Event::Ptr event = Event::Ptr(new Event());

        const auto timeZone = m_variables->getCalendar()->timeZone();
        event->setDtStart(m_variables->getStartDateTime().toTimeZone(timeZone));
        event->setDtEnd(m_variables->getEndDateTime().toTimeZone(timeZone));
        event->setSummary(m_variables->getSummary());
        event->setAllDay(m_variables->getFloating());
        event->setDescription(m_variables->getDescription());
        event->setLocation(m_variables->getLocation());

        Akonadi::CalendarBase::Ptr calendar = m_variables->getCalendar();
        QEventLoop loop;
        QObject::connect(calendar.data(), &Akonadi::CalendarBase::createFinished,
                         &loop, &QEventLoop::quit);
        QElapsedTimer t;
        t.start();
        Q_ASSERT(calendar->incidence(event->uid()) == 0);  // can't exist yet
        if (!m_variables->allowGui()) {
            Akonadi::IncidenceChanger *changer = calendar->incidenceChanger();
            changer->setShowDialogsOnError(false);
            Akonadi::Collection collection = m_variables->collectionId() != -1 ? Akonadi::Collection(m_variables->collectionId())
                                             : Akonadi::Collection(CalendarSupport::KCalPrefs::instance()->defaultCalendarId());

            if (!collection.isValid()) {
                cout << i18n("Calendar is invalid. Please specify one with --calendar").toLocal8Bit().data() << "\n";
            }

            changer->setDefaultCollection(collection);
            changer->setDestinationPolicy(Akonadi::IncidenceChanger::DestinationPolicyNeverAsk);
        }
        calendar->addEvent(event);
        loop.exec();
        qCDebug(KONSOLEKALENDAR_LOG) << "Creation took " << t.elapsed() << "ms.";
        status = calendar->incidence(event->uid()) != 0;
        if (status) {
            cout << i18n("Success: \"%1\" inserted",
                         m_variables->getSummary()).toLocal8Bit().data()
                 << endl;

        } else {
            cout << i18n("Failure: \"%1\" not inserted",
                         m_variables->getSummary()).toLocal8Bit().data()
                 << endl;
            status = false;
        }
    }

    qCDebug(KONSOLEKALENDAR_LOG) << "konsolekalendaradd.cpp::addEvent() | Done";
    return status;
}

bool KonsoleKalendarAdd::addImportedCalendar()
{
    MemoryCalendar::Ptr cal(new MemoryCalendar(QTimeZone::utc()));
    FileStorage instore(cal, m_variables->getImportFile());
    if (!instore.load()) {
        qCDebug(KONSOLEKALENDAR_LOG) << "konsolekalendaradd.cpp::importCalendar() |"
            << "Can't import file:"
            << m_variables->getImportFile();
        return false;
    }
    Akonadi::CalendarBase::Ptr calendar = m_variables->getCalendar();

    if (!m_variables->allowGui()) {
        Akonadi::IncidenceChanger *changer = calendar->incidenceChanger();
        changer->setShowDialogsOnError(false);
        Akonadi::Collection collection = m_variables->collectionId() != -1 ? Akonadi::Collection(m_variables->collectionId())
        : Akonadi::Collection(CalendarSupport::KCalPrefs::instance()->defaultCalendarId());

        if (!collection.isValid()) {
            cout << i18n("Calendar is invalid. Please specify one with --calendar").toLocal8Bit().data() << "\n";
        }

        changer->setDefaultCollection(collection);
        changer->setDestinationPolicy(Akonadi::IncidenceChanger::DestinationPolicyNeverAsk);
    }

    QEventLoop loop;
    QObject::connect(calendar.data(), &Akonadi::CalendarBase::createFinished,
                        &loop, &QEventLoop::quit);
    QElapsedTimer t;
    foreach(const auto &event, cal->rawEvents()) {
        if (calendar->incidence(event->uid()) != 0) {
            if (m_variables->isVerbose()) {
                cout << i18n("Insert Event skipped, because UID \"%1\" is already known. <Verbose>", event->uid()).toLocal8Bit().data()
                    << endl;
            } else {
                qCInfo(KONSOLEKALENDAR_LOG) << "Event with UID " << event->uid() << "is already in calendar, skipping import of this Event.";
            }
            continue;
        }
        if (m_variables->isVerbose()) {
            cout << i18n("Add Event with UID \"%1\". <Verbose>", event->uid()).toLocal8Bit().data()
                << endl;
        }
        if (m_variables->isDryRun()) {
            continue;
        }
        t.start();
        calendar->addEvent(event);
        loop.exec();
        qCDebug(KONSOLEKALENDAR_LOG) << "Creation of event took " << t.elapsed() << "ms." << "status: "<< (calendar->incidence(event->uid()) != 0);
        if (calendar->incidence(event->uid()) == 0) {
            return false;
        }
    }
    qCDebug(KONSOLEKALENDAR_LOG) << "konsolekalendaradd.cpp::importCalendar() |"
        << "Successfully imported file:"
        << m_variables->getImportFile();
    return true;
}

void KonsoleKalendarAdd::printSpecs()
{
    cout << i18n("  What:  %1",
                 m_variables->getSummary()).toLocal8Bit().data()
         << endl;

    cout << i18n("  Begin: %1",
                 m_variables->getStartDateTime().toString(Qt::TextDate)).toLocal8Bit().data()
         << endl;

    cout << i18n("  End:   %1",
                 m_variables->getEndDateTime().toString(Qt::TextDate)).toLocal8Bit().data()
         << endl;

    if (m_variables->getFloating() == true) {
        cout << i18n("  No Time Associated with Event").toLocal8Bit().data()
             << endl;
    }

    cout << i18n("  Desc:  %1",
                 m_variables->getDescription()).toLocal8Bit().data()
         << endl;

    cout << i18n("  Location:  %1",
                 m_variables->getLocation()).toLocal8Bit().data()
         << endl;
}
