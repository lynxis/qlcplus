/*
  Q Light Controller
  audioeditor.cpp

  Copyright (c) Massimo Callegari

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

#include <QLineEdit>
#include <QLabel>
#include <QDebug>


#include "audiodecoder.h"
#include "audioeditor.h"
#include "audio.h"
#include "doc.h"

AudioEditor::AudioEditor(QWidget* parent, Audio *audio, Doc* doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_audio(audio)
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(audio != NULL);

    setupUi(this);

    m_nameEdit->setText(m_audio->name());
    m_nameEdit->setSelection(0, m_nameEdit->text().length());

    m_fadeInEdit->setText(Function::speedToString(audio->fadeInSpeed()));
    m_fadeOutEdit->setText(Function::speedToString(audio->fadeOutSpeed()));

    connect(m_nameEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(slotNameEdited(const QString&)));

    connect(m_fadeInEdit, SIGNAL(returnPressed()),
            this, SLOT(slotFadeInEdited()));
    connect(m_fadeOutEdit, SIGNAL(returnPressed()),
            this, SLOT(slotFadeOutEdited()));

    AudioDecoder *adec = m_audio->getAudioDecoder();

    m_filenameLabel->setText(m_audio->getSourceFileName());
    if (adec != NULL)
    {
        AudioParameters ap = adec->audioParameters();
        m_durationLabel->setText(Function::speedToString(m_audio->getDuration()));
        m_srateLabel->setText(QString("%1 Hz").arg(ap.sampleRate()));
        m_channelsLabel->setText(QString("%1").arg(ap.channels()));
        m_bitrateLabel->setText(QString("%1 kb/s").arg(adec->bitrate()));
    }

    // Set focus to the editor
    m_nameEdit->setFocus();
}

AudioEditor::~AudioEditor()
{
}

void AudioEditor::slotNameEdited(const QString& text)
{
    m_audio->setName(text);
    m_doc->setModified();
}

void AudioEditor::slotFadeInEdited()
{
    uint newValue;
    QString text = m_fadeInEdit->text();
    if (text.contains(".") || text.contains("s") ||
        text.contains("m") || text.contains("h"))
            newValue = Function::stringToSpeed(text);
    else
    {
        newValue = (text.toDouble() * 1000);
        m_fadeInEdit->setText(Function::speedToString(newValue));
    }

    m_audio->setFadeInSpeed(newValue);
    m_doc->setModified();
}

void AudioEditor::slotFadeOutEdited()
{
    uint newValue;
    QString text = m_fadeOutEdit->text();
    if (text.contains(".") || text.contains("s") ||
        text.contains("m") || text.contains("h"))
            newValue = Function::stringToSpeed(text);
    else
    {
        newValue = (text.toDouble() * 1000);
        m_fadeOutEdit->setText(Function::speedToString(newValue));
    }

    m_audio->setFadeOutSpeed(newValue);
    m_doc->setModified();
}
