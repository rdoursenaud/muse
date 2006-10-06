//=============================================================================
//  MusE
//  Linux Music Editor
//  $Id:$
//
//  Copyright (C) 2002-2006 by Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#ifndef __AUDIOTRACK_H__
#define __AUDIOTRACK_H__

#include "fifo.h"
#include "track.h"

class PluginI;

//---------------------------------------------------------
//   AuxSend
//---------------------------------------------------------

struct AuxSend {
      bool postfader;
      };

//---------------------------------------------------------
//   AudioTrack
//---------------------------------------------------------

class AudioTrack : public Track {
      Q_OBJECT

      bool _prefader;               // prefader metering
      Pipeline* _efxPipe;
      QList<AuxSend> sends;

      void readRecfile(QDomNode);

   protected:
      float** buffer;               // this buffer is filled by process()
                                    // _volume and _pan is not applied

      bool bufferEmpty;			// set by process() to optimize
      					// data flow

      SndFile* _recFile;
      Fifo fifo;                    // fifo -> _recFile

      virtual bool setMute(bool val);
      virtual bool setOff(bool val);
      virtual bool setSolo(bool val);
      virtual void collectInputData();

   private slots:
      virtual void setAutoRead(bool);
      virtual void setAutoWrite(bool);

   public:
      AudioTrack(TrackType t);
      virtual ~AudioTrack();

      bool readProperties(QDomNode);
      void writeProperties(Xml&) const;

      virtual Part* newPart(Part*p=0, bool clone=false);

      SndFile* recFile() const           { return _recFile; }
      void setRecFile(SndFile* sf)       { _recFile = sf;   }

      virtual void setChannels(int n);

      virtual bool isMute() const;

      void putFifo(int channels, unsigned long n, float** bp);

      void record();
      virtual void startRecording();
      virtual void stopRecording(const AL::Pos&, const AL::Pos&) {}

      bool prefader() const              { return _prefader; }
      void addAuxSend(int n);
      void setPrefader(bool val);
      Pipeline* efxPipe()                { return _efxPipe;  }
      void addPlugin(PluginI* plugin, int idx);
      PluginI* plugin(int idx) const;

      virtual bool hasAuxSend() const { return false; }
      virtual void process();
      void multiplyAdd(int channel,  float**, int bus);
      bool multiplyCopy(int channel, float**, int bus);
      };

#endif
