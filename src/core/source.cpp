#include "core/source.h"

void ReceiverSource::startRecording(const QString &)
{
    emit recordingChanged(false, tr("Recording is only available for a serial port."));
}

void ReceiverSource::stopRecording() {}
