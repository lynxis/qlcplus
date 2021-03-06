/*
  Q Light Controller
  mastertimer.cpp

  Copyright (C) Heikki Junnila

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QDebug>
#include <QMutexLocker>

#if defined(WIN32) || defined(Q_OS_WIN)
#   include "mastertimer-win32.h"
#else
#   include <unistd.h>
#   include "mastertimer-unix.h"
#endif

#include "universearray.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "mastertimer.h"
#include "outputmap.h"
#include "dmxsource.h"
#include "qlcmacros.h"
#include "function.h"
#include "doc.h"

#define MASTERTIMER_FREQUENCY "mastertimer/frequency"

/** The timer tick frequency in Hertz */
uint MasterTimer::s_frequency = 50;
uint MasterTimer::s_tick = 20;

/*****************************************************************************
 * Initialization
 *****************************************************************************/

MasterTimer::MasterTimer(Doc* doc)
    : QObject(doc)
    , m_stopAllFunctions(false)
    , m_fader(new GenericFader(doc))
    , d_ptr(new MasterTimerPrivate(this))
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(d_ptr != NULL);

    QSettings settings;
    QVariant var = settings.value(MASTERTIMER_FREQUENCY);
    if (var.isValid() == true)
        s_frequency = var.toUInt();

    s_tick = uint(double(1000) / double(s_frequency));
}

MasterTimer::~MasterTimer()
{
    if (d_ptr->isRunning() == true)
        stop();

    delete d_ptr;
    d_ptr = NULL;
}

void MasterTimer::start()
{
    Q_ASSERT(d_ptr != NULL);
    d_ptr->start();
}

void MasterTimer::stop()
{
    Q_ASSERT(d_ptr != NULL);
    stopAllFunctions();
    d_ptr->stop();
}

void MasterTimer::timerTick()
{
    Doc* doc = qobject_cast<Doc*> (parent());
    Q_ASSERT(doc != NULL);

    UniverseArray* universes = doc->outputMap()->claimUniverses();
    universes->zeroIntensityChannels();
    universes->zeroRelativeValues();

    timerTickFunctions(universes);
    timerTickDMXSources(universes);
    timerTickFader(universes);

    doc->outputMap()->releaseUniverses();
    doc->outputMap()->dumpUniverses();
}

uint MasterTimer::frequency()
{
    return s_frequency;
}

uint MasterTimer::tick()
{
    return s_tick;
}

/*****************************************************************************
 * Functions
 *****************************************************************************/

void MasterTimer::startFunction(Function* function)
{
    if (function == NULL)
        return;

    QMutexLocker locker(&m_functionListMutex);
    if (m_startQueue.contains(function) == false)
        m_startQueue.append(function);
}

void MasterTimer::stopAllFunctions()
{
    m_stopAllFunctions = true;

    /* Wait until all functions have been stopped */
    while (runningFunctions() > 0)
    {
#if defined(WIN32) || defined(Q_OS_WIN)
        Sleep(10);
#else
        usleep(10000);
#endif
    }

    // WARNING: the following brackets are fundamental for
    // the scope of this piece of code !!
    {
        /* Remove all generic fader's channels */
        QMutexLocker functionLocker(&m_functionListMutex);
        QMutexLocker dmxLocker(&m_dmxSourceListMutex);
        fader()->removeAll();
    }

    m_stopAllFunctions = false;
}

void MasterTimer::fadeAndStopAll(int timeout)
{
    if (timeout == 0)
        return;

    Doc* doc = qobject_cast<Doc*> (parent());
    Q_ASSERT(doc != NULL);

    UniverseArray* universes = doc->outputMap()->claimUniverses();
    QHashIterator <int,uchar> it(universes->intensityChannels());
    while (it.hasNext() == true)
    {
        it.next();

        Fixture* fxi = doc->fixture(doc->fixtureForAddress(it.key()));
        if (fxi != NULL)
        {
            uint ch = it.key() - fxi->universeAddress();
            if (fxi->channelCanFade(ch))
            {
                FadeChannel fc;
                fc.setFixture(fxi->id());
                fc.setChannel(ch);
                fc.setStart(it.value());
                fc.setTarget(0);
                fc.setFadeTime(timeout);
                fader()->add(fc);
            }
        }
    }
    doc->outputMap()->releaseUniverses();
    m_stopAllFunctions = true;
}

int MasterTimer::runningFunctions() const
{
    return m_functionList.size();
}

void MasterTimer::timerTickFunctions(UniverseArray* universes)
{
    // List of m_functionList indices that should be removed at the end of this
    // function. The functions at the indices have been stopped.
    QList <int> removeList;

    /* Lock before accessing the running functions list. */
    m_functionListMutex.lock();
    for (int i = 0; i < m_functionList.size(); i++)
    {
        Function* function = m_functionList.at(i);

        /* No need to access function list on this round anymore */
        m_functionListMutex.unlock();

        if (function != NULL)
        {
            /* Run the function unless it's supposed to be stopped */
            if (function->stopped() == false && m_stopAllFunctions == false)
            {
                function->write(this, universes);
            }
            else
            {
                /* Function should be stopped instead */
                m_functionListMutex.lock();
                function->postRun(this, universes);
                //qDebug() << "[MasterTimer] Add function (ID: " << function->id() << ") to remove list ";
                removeList << i; // Don't remove the item from the list just yet.
                m_functionListMutex.unlock();
                emit functionListChanged();
            }
        }

        /* Lock function list for the next round. */
        m_functionListMutex.lock();
    }

    // Remove functions that need to be removed AFTER all functions have been run
    // for this round. This is done separately to prevent a case when a function
    // is first removed and then another is added (chaser, for example), keeping the
    // list's size the same, thus preventing the last added function from being run
    // on this round. The indices in removeList are automatically sorted because the
    // list is iterated with an int above from 0 to size, so iterating the removeList
    // backwards here will always remove the correct indices.
    QListIterator <int> it(removeList);
    it.toBack();
    while (it.hasPrevious() == true)
        m_functionList.removeAt(it.previous());

    foreach (Function* f, m_startQueue)
    {
        //qDebug() << "[MasterTimer] Processing ID: " << f->id();
        if (m_functionList.contains(f) == false)
        {
            m_functionList.append(f);
            m_functionListMutex.unlock();
            //qDebug() << "[MasterTimer] Starting up ID: " << f->id();
            f->preRun(this);
            f->write(this, universes);
            emit functionListChanged();
            m_functionListMutex.lock();
        }
        m_startQueue.removeOne(f);
    }

    /* No more functions. Get out and wait for next timer event. */
    m_functionListMutex.unlock();
}

/****************************************************************************
 * DMX Sources
 ****************************************************************************/

void MasterTimer::registerDMXSource(DMXSource* source)
{
    Q_ASSERT(source != NULL);

    QMutexLocker lock(&m_dmxSourceListMutex);
    if (m_dmxSourceList.contains(source) == false)
        m_dmxSourceList.append(source);
}

void MasterTimer::unregisterDMXSource(DMXSource* source)
{
    Q_ASSERT(source != NULL);

    QMutexLocker lock(&m_dmxSourceListMutex);
    m_dmxSourceList.removeAll(source);
}

void MasterTimer::timerTickDMXSources(UniverseArray* universes)
{
    /* Lock before accessing the DMX sources list. */
    m_dmxSourceListMutex.lock();
    for (int i = 0; i < m_dmxSourceList.size(); i++)
    {
        DMXSource* source = m_dmxSourceList.at(i);
        Q_ASSERT(source != NULL);

        /* No need to access the list on this round anymore. */
        m_dmxSourceListMutex.unlock();

        /* Get DMX data from the source */
        source->writeDMX(this, universes);

        /* Lock for the next round. */
        m_dmxSourceListMutex.lock();
    }

    /* No more sources. Get out and wait for next timer event. */
    m_dmxSourceListMutex.unlock();
}

/****************************************************************************
 * Generic Fader
 ****************************************************************************/

GenericFader* MasterTimer::fader() const
{
    return m_fader;
}

void MasterTimer::timerTickFader(UniverseArray* universes)
{
    QMutexLocker functionLocker(&m_functionListMutex);
    QMutexLocker dmxLOcker(&m_dmxSourceListMutex);

    fader()->write(universes);
}
