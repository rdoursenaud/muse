//=============================================================================
//  MusE
//  Linux Music Editor
//
//  lv2host.h
//  Copyright (C) 2014 by Deryabin Andrew <andrewderyabin@gmail.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//=============================================================================

#ifndef __LV2HOST_H__
#define __LV2HOST_H__

#include "config.h"

#ifdef LV2_SUPPORT

//uncomment to print debugging information for lv2 host
//#define DEBUG_LV2

#include "lilv/lilv.h"
#include "lv2/lv2plug.in/ns/ext/data-access/data-access.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/buf-size/buf-size.h"
#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/options/options.h"
#include "lv2/lv2plug.in/ns/ext/parameters/parameters.h"
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include "lv2/lv2plug.in/ns/ext/port-groups/port-groups.h"
#include "lv2/lv2plug.in/ns/ext/presets/presets.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/ext/worker/worker.h"
#include "lv2/lv2plug.in/ns/ext/port-props/port-props.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/log/log.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "lv2extui.h"
#include "lv2extprg.h"

#include <cstring>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <QMutex>
#include <QSemaphore>
#include <QThread>
#include <QTimer>
#include <assert.h>
#include <algorithm>
#include <alsa/asoundlib.h>
#include "midictrl.h"
#include "synth.h"
#include "stringparam.h"

#include "plugin.h"

#endif

namespace MusECore
{

#ifdef LV2_SUPPORT

/* LV2EvBuf class is based of lv2_evbuf_* functions
 * from jalv lv2 plugin host
 *
 * Copyright 2008-2012 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

class LV2EvBuf
{

public:

    /**
       Format of actual buffer.
    */
    typedef enum
   {
        /**
           An (old) ev:EventBuffer (LV2_Event_Buffer).
        */
        LV2_EVBUF_EVENT,

        /**
           A (new) atom:Sequence (LV2_Atom_Sequence).
        */
        LV2_EVBUF_ATOM
    } LV2_Evbuf_Type;

    typedef struct
   {
        LV2_Evbuf_Type type;
        uint32_t       capacity;
        uint32_t       atom_Chunk;
        uint32_t       atom_Sequence;
        union {
            LV2_Event_Buffer  event;
            LV2_Atom_Sequence atom;
        } buf;
    } LV2_Evbuf;

    /**
    An iterator over an LV2_Evbuf.
    */
    class LV2_Evbuf_Iterator
    {
    private:
        LV2EvBuf *lv2evbuf;

    public:
        uint32_t   offset;
        LV2_Evbuf_Iterator ( LV2EvBuf *a, uint32_t b ) : lv2evbuf ( a ), offset ( b )
        {

        }
        bool
        lv2_evbuf_is_valid ()
        {
            return offset < lv2evbuf->lv2_evbuf_get_size ();
        }
        LV2EvBuf *operator *()
        {
            return lv2evbuf;
        }
        LV2_Evbuf_Iterator &operator++ ()
        {
            if ( !lv2_evbuf_is_valid () )
            {
                return *this;
            }

            LV2_Evbuf *_evbuf  = lv2evbuf->evbuf;
            uint32_t   _offset = offset;
            uint32_t   _size;
            switch ( _evbuf->type )
            {
            case LV2_EVBUF_EVENT:
                _size    = ( ( LV2_Event * ) ( _evbuf->buf.event.data + _offset ) )->size;
                _offset += LV2EvBuf::lv2_evbuf_pad_size ( sizeof ( LV2_Event ) + _size );
                break;
            case LV2_EVBUF_ATOM:
                _size = ( ( LV2_Atom_Event * )
                          ( ( char * ) LV2_ATOM_CONTENTS ( LV2_Atom_Sequence, &_evbuf->buf.atom ) + _offset ) )->body.size;
                _offset += LV2EvBuf::lv2_evbuf_pad_size ( sizeof ( LV2_Atom_Event ) + _size );
                break;
            }
            offset = _offset;
            return *this;
        }
        LV2_Event *event()
        {
            LV2_Event_Buffer *ebuf = &lv2evbuf->evbuf->buf.event;
            return ( LV2_Event * ) ( ( char * ) ebuf->data + offset );
        }

        LV2_Atom_Event *aevent()
        {
            LV2_Atom_Sequence *aseq = ( LV2_Atom_Sequence * ) &lv2evbuf->evbuf->buf.atom;
            return ( LV2_Atom_Event * ) (( char * ) LV2_ATOM_CONTENTS ( LV2_Atom_Sequence, aseq ) + offset );
        }
    };

    LV2_Evbuf *evbuf;

    static inline uint32_t
    lv2_evbuf_pad_size ( uint32_t size )
    {
        return ( size + 7 ) & ( ~7 );
    }

    LV2EvBuf ( uint32_t       capacity,
               LV2_Evbuf_Type type,
               uint32_t       atom_Chunk,
               uint32_t       atom_Sequence )
    {
       size_t sz = sizeof ( LV2_Evbuf ) + sizeof ( LV2_Atom_Sequence ) + capacity;
       int rv = posix_memalign ( ( void ** ) &evbuf, 8, sz );
       if ( rv != 0 )
       {
          fprintf ( stderr, "ERROR: LV2EvBuf::LV2EvBuf: posix_memalign returned error:%d. Aborting!\n", rv );
          abort();
       }
       memset(evbuf, 0, sz);
       evbuf->capacity      = capacity;
       evbuf->atom_Chunk    = atom_Chunk;
       evbuf->atom_Sequence = atom_Sequence;
       lv2_evbuf_set_type ( type );
       lv2_evbuf_reset ( true );
    }

    ~LV2EvBuf()
    {
        free ( evbuf );
    }

    void
    lv2_evbuf_set_type ( LV2_Evbuf_Type type )
    {
        evbuf->type = type;
        switch ( type ) {
        case LV2_EVBUF_EVENT:
            evbuf->buf.event.data     = ( uint8_t * ) ( evbuf + 1 );
            evbuf->buf.event.capacity = evbuf->capacity;
            break;
        case LV2_EVBUF_ATOM:
            break;
        }
        lv2_evbuf_reset (true);
    }

    void
    lv2_evbuf_reset (bool input)
    {
        switch ( evbuf->type ) {
        case LV2_EVBUF_EVENT:
            evbuf->buf.event.header_size = sizeof ( LV2_Event_Buffer );
            evbuf->buf.event.stamp_type  = LV2_EVENT_AUDIO_STAMP;
            evbuf->buf.event.event_count = 0;
            evbuf->buf.event.size        = 0;
            break;
        case LV2_EVBUF_ATOM:
            if(input)
            {
                evbuf->buf.atom.atom.size = sizeof ( LV2_Atom_Sequence_Body );
                evbuf->buf.atom.atom.type = evbuf->atom_Sequence;
            }
            else
            {
                evbuf->buf.atom.atom.size = evbuf->capacity;
                evbuf->buf.atom.atom.type = evbuf->atom_Chunk;
            }
        }
    }

    uint32_t
    lv2_evbuf_get_size ()
    {
        switch ( evbuf->type )
        {
        case LV2_EVBUF_EVENT:
            return evbuf->buf.event.size;
        case LV2_EVBUF_ATOM:
            assert ( evbuf->buf.atom.atom.type != evbuf->atom_Sequence
                     || evbuf->buf.atom.atom.size >= sizeof ( LV2_Atom_Sequence_Body ) );
            return evbuf->buf.atom.atom.type == evbuf->atom_Sequence
                   ? evbuf->buf.atom.atom.size - sizeof ( LV2_Atom_Sequence_Body )
                   : 0;
        }
        return 0;
    }

    void *
    lv2_evbuf_get_buffer ()
    {
        switch ( evbuf->type )
        {
        case LV2_EVBUF_EVENT:
            return &evbuf->buf.event;
        case LV2_EVBUF_ATOM:
            return &evbuf->buf.atom;
        }
        return NULL;
    }

    LV2_Evbuf_Iterator
    lv2_evbuf_begin ()
    {
        LV2_Evbuf_Iterator iter ( this, 0 );
        return iter;
    }

    LV2_Evbuf_Iterator
    lv2_evbuf_end ()
    {
        const uint32_t           size = lv2_evbuf_get_size ();
        const LV2_Evbuf_Iterator iter ( this, lv2_evbuf_pad_size ( size ) );
        return iter;
    }

    bool
    lv2_evbuf_get ( LV2_Evbuf_Iterator &iter,
                    uint32_t          *frames,
                    uint32_t          *subframes,
                    uint32_t          *type,
                    uint32_t          *size,
                    uint8_t          **data ) const
    {
        *frames = *subframes = *type = *size = 0;
        *data = NULL;

        if ( !iter.lv2_evbuf_is_valid() )
        {
            return false;
        }

        LV2_Event         *ev;
        LV2_Atom_Event    *aev;
        switch ( ( *iter )->evbuf->type )
        {
        case LV2_EVBUF_EVENT:
            ev = iter.event();
            *frames    = ev->frames;
            *subframes = ev->subframes;
            *type      = ev->type;
            *size      = ev->size;
            *data      = ( uint8_t * ) ev + sizeof ( LV2_Event );
            break;
        case LV2_EVBUF_ATOM:
            aev = iter.aevent();
            *frames    = aev->time.frames;
            *subframes = 0;
            *type      = aev->body.type;
            *size      = aev->body.size;
            *data      = ( uint8_t * ) LV2_ATOM_BODY ( &aev->body );
            break;
        }

        return true;
    }

    bool
    lv2_evbuf_write ( LV2_Evbuf_Iterator &iter,
                      uint32_t            frames,
                      uint32_t            subframes,
                      uint32_t            type,
                      uint32_t            size,
                      const uint8_t      *data ) const
    {
        LV2_Event_Buffer  *ebuf;
        LV2_Event         *ev;
        LV2_Atom_Sequence *aseq;
        LV2_Atom_Event    *aev;
        switch ( ( *iter )->evbuf->type )
        {
        case LV2_EVBUF_EVENT:
            ebuf = & ( *iter )->evbuf->buf.event;
            if ( ebuf->capacity - ebuf->size < sizeof ( LV2_Event ) + size )
            {
                return false;
            }

            ev = ( LV2_Event * ) ( ebuf->data + iter.offset );
            ev->frames    = frames;
            ev->subframes = subframes;
            ev->type      = type;
            ev->size      = size;
            memcpy ( ( uint8_t * ) ev + sizeof ( LV2_Event ), data, size );

            size               = lv2_evbuf_pad_size ( sizeof ( LV2_Event ) + size );
            ebuf->size        += size;
            ebuf->event_count += 1;
            iter.offset      += size;
            break;
        case LV2_EVBUF_ATOM:
            aseq = ( LV2_Atom_Sequence * ) & ( *iter )->evbuf->buf.atom;
            if ( ( *iter )->evbuf->capacity - sizeof ( LV2_Atom ) - aseq->atom.size < sizeof ( LV2_Atom_Event ) + size )
            {
                return false;
            }

            aev = ( LV2_Atom_Event * ) (( char * ) LV2_ATOM_CONTENTS ( LV2_Atom_Sequence, aseq ) + iter.offset );
            aev->time.frames = frames;
            aev->body.type   = type;
            aev->body.size   = size;
            memcpy ( LV2_ATOM_BODY ( &aev->body ), data, size );
            size             = lv2_evbuf_pad_size ( sizeof ( LV2_Atom_Event ) + size );
            aseq->atom.size += size;
            iter.offset    += size;
            break;
        }

        return true;
    }

};

#define LV2_RT_FIFO_ITEM_SIZE (std::max(size_t(4096 * 16), size_t(MusEGlobal::segmentSize * 16)))

class LV2SimpleRTFifo
{
public:
   typedef struct _lv2_uiControlEvent
   {
      uint32_t port_index;
      long buffer_size;
      char *data;
   } lv2_uiControlEvent;
private:
   size_t numItems;
   std::vector<lv2_uiControlEvent> eventsBuffer;
   size_t readIndex;
   size_t writeIndex;
   size_t fifoSize;
   size_t itemSize;
public:
   LV2SimpleRTFifo(size_t size):
      fifoSize(size),
      itemSize(LV2_RT_FIFO_ITEM_SIZE)
   {
      eventsBuffer.resize(fifoSize);
      assert(eventsBuffer.size() == fifoSize);
      readIndex = writeIndex = 0;
      for(size_t i = 0; i < fifoSize; ++i)
      {
         eventsBuffer [i].port_index = 0;
         eventsBuffer [i].buffer_size = 0;
         eventsBuffer [i].data = new char [itemSize];
      }

   }
   ~LV2SimpleRTFifo()
   {
      for(size_t i = 0; i < fifoSize; ++i)
      {
         delete [] eventsBuffer [i].data;
      }
   }
   inline size_t getItemSize(){return itemSize; }
   bool put(uint32_t port_index, uint32_t size, const void *data)
   {      
      if(size > itemSize)
      {
#ifdef DEBUG_LV2
         std::cerr << "LV2SimpleRTFifo:put(): size("<<size<<") is too big" << std::endl;
#endif
         return false;

      }
      size_t i = writeIndex;
      bool found = false;
      do
      {
         if(eventsBuffer.at(i).buffer_size == 0)
         {
            found = true;
            break;
         }
         i++;
         i %= fifoSize;
      }
      while(i != writeIndex);

      if(!found)
      {
#ifdef DEBUG_LV2
         std::cerr << "LV2SimpleRTFifo:put(): fifo is full" << std::endl;
#endif
         return false;
      }
#ifdef DEBUG_LV2
     // std::cerr << "LV2SimpleRTFifo:put(): used index = " << i << std::endl;
#endif
      memcpy(eventsBuffer.at(i).data, data, size);
      eventsBuffer.at(i).port_index = port_index;
      __sync_fetch_and_add(&eventsBuffer.at(i).buffer_size, size);
      writeIndex = (i + 1) % fifoSize;

      return true;

   }

   bool get(uint32_t *port_index, size_t *szOut, char *data_out)
   {
      size_t i = readIndex;
      bool found = false;
      do
      {
         if(eventsBuffer.at(i).buffer_size != 0)
         {
            found = true;
            break;
         }
         i++;
         i %= fifoSize;
      }
      while(i != readIndex);

      if(!found)
      {
#ifdef DEBUG_LV2
         //std::cerr << "LV2SimpleRTFifo:get(): fifo is empty" << std::endl;
#endif
         return NULL;
      }
#ifdef DEBUG_LV2
      //std::cerr << "LV2SimpleRTFifo:get(): used index = " << i << std::endl;
#endif
      *szOut = eventsBuffer.at(i).buffer_size;
      *port_index = eventsBuffer [i].port_index;
      memcpy(data_out, eventsBuffer [i].data, *szOut);
      __sync_fetch_and_and(&eventsBuffer.at(i).buffer_size, 0);
      readIndex = (i + 1) % fifoSize;
      return true;
   }


};



struct LV2MidiPort
{
    LV2MidiPort ( const LilvPort *_p, uint32_t _i, QString _n, bool _f, LV2EvBuf *_b, bool _supportsTimePos ) :
        port ( _p ), index ( _i ), name ( _n ), old_api ( _f ), buffer ( _b ), supportsTimePos(_supportsTimePos) {}
    const LilvPort *port;
    uint32_t index; //plugin real port index
    QString name;
    bool old_api; //true for LV2_Event port
    LV2EvBuf *buffer;
    bool supportsTimePos;
    ~LV2MidiPort()
    {
#if 0
        std::cerr << "~LV2MidiPort()" << std::endl;
#endif
        if(buffer != NULL)
            delete buffer;
        buffer = NULL;
    }
};

enum LV2ControlPortType
{
   LV2_PORT_DISCRETE = 1,
   LV2_PORT_INTEGER,
   LV2_PORT_CONTINUOUS,
   LV2_PORT_LOGARITHMIC,
   LV2_PORT_TRIGGER
};

struct LV2ControlPort
{
   LV2ControlPort ( const LilvPort *_p, uint32_t _i, float _c, const char *_n, const char *_s, LV2ControlPortType _ctype, bool _isCVPort = false) :
      port ( _p ), index ( _i ), defVal ( _c ), minVal( _c ), maxVal ( _c ), cType(_ctype),
      isCVPort(_isCVPort)
   {
      cName = strdup ( _n );
      cSym = strdup(_s);
   }
   LV2ControlPort ( const LV2ControlPort &other ) :
      port ( other.port ), index ( other.index ), defVal ( other.defVal ),
      minVal(other.minVal), maxVal(other.maxVal), cType(other.cType),
      isCVPort(other.isCVPort)
   {
      cName = strdup ( other.cName );
      cSym = strdup(other.cSym);
   }
   ~LV2ControlPort()
   {
      free ( cName );      
      cName = NULL;
      free(cSym);
      cSym = NULL;
   }
   const LilvPort *port;
   uint32_t index; //plugin real port index
   float defVal; //default control value
   float minVal; //minimum control value
   float maxVal; //maximum control value
   char *cName; //cached value to share beetween function calls
   char *cSym; //cached port symbol
   LV2ControlPortType cType;
   bool isCVPort;
};

struct LV2AudioPort
{
    LV2AudioPort ( const LilvPort *_p, uint32_t _i, float *_b, QString _n ) :
        port ( _p ), index ( _i ), buffer ( _b ), name ( _n ) {}
    const LilvPort *port;
    uint32_t index; //plugin real port index
    float *buffer; //audio buffer
    QString name;
};

struct cmp_str
{
    bool operator() ( char const *a, char const *b ) const
    {
        return std::strcmp ( a, b ) < 0;
    }
};

typedef std::vector<LV2MidiPort> LV2_MIDI_PORTS;
typedef std::vector<LV2ControlPort> LV2_CONTROL_PORTS;
typedef std::vector<LV2AudioPort> LV2_AUDIO_PORTS;
typedef std::map<const char *, uint32_t, cmp_str> LV2_SYNTH_URID_MAP;
typedef std::map<uint32_t, const char *> LV2_SYNTH_URID_RMAP;

class LV2UridBiMap
{
private:
    LV2_SYNTH_URID_MAP _map;
    LV2_SYNTH_URID_RMAP _rmap;
    uint32_t nextId;
    QMutex idLock;
public:
    LV2UridBiMap() : nextId ( 1 ) {_map.clear(); _rmap.clear();}
    ~LV2UridBiMap()
    {
       LV2_SYNTH_URID_MAP::iterator it = _map.begin();
       for(;it != _map.end(); ++it)
       {
          free((void*)(*it).first);
       }
    }

    LV2_URID map ( const char *uri ) {
        std::pair<LV2_SYNTH_URID_MAP::iterator, bool> p;
        uint32_t id;        
        idLock.lock();
        LV2_SYNTH_URID_MAP::iterator it = _map.find(uri);
        if(it == _map.end())
        {
            const char *mUri = strdup(uri);
            p = _map.insert ( std::make_pair ( mUri, nextId ) );
            _rmap.insert ( std::make_pair ( nextId, mUri ) );
            nextId++;
            id = p.first->second;
        }
        else
           id = it->second;
        idLock.unlock();
        return id;

    }
    const char *unmap ( uint32_t id ) {
        LV2_SYNTH_URID_RMAP::iterator it = _rmap.find ( id );
        if ( it != _rmap.end() ) {
            return it->second;
        }

        return NULL;
    }
};

class LV2SynthIF;
class LV2PluginWrapper_State;

typedef std::map<const LilvUI *, std::pair<bool, const LilvNode *> > LV2_PLUGIN_UI_TYPES;

class LV2Synth : public Synth
{
private:
    const LilvPlugin *_handle;
    LV2UridBiMap uridBiMap;
    LV2_Feature *_features;
    LV2_Feature **_ppfeatures;
    LV2_Options_Option *_options;
    LV2_URID_Map _lv2_urid_map;
    LV2_URID_Unmap _lv2_urid_unmap;
    LV2_URI_Map_Feature _lv2_uri_map;
    LV2_Log_Log _lv2_log_log;    
    double _sampleRate;
    bool _isSynth;
    int _uniqueID;
    uint32_t _midi_event_id;
    LilvUIs *_uis;
    std::map<uint32_t, uint32_t> _idxToControlMap;

    LV2_PLUGIN_UI_TYPES _pluginUiTypes;

    //templates for LV2SynthIF and LV2PluginWrapper instantiation
    LV2_MIDI_PORTS _midiInPorts;
    LV2_MIDI_PORTS _midiOutPorts;
    LV2_CONTROL_PORTS _controlInPorts;
    LV2_CONTROL_PORTS _controlOutPorts;
    LV2_AUDIO_PORTS _audioInPorts;
    LV2_AUDIO_PORTS _audioOutPorts;

    MidiCtl2LadspaPortMap midiCtl2PortMap;   // Maps midi controller numbers to LV2 port numbers.
    MidiCtl2LadspaPortMap port2MidiCtlMap;   // Maps LV2 port numbers to midi controller numbers.
    uint32_t _fInstanceAccess;
    uint32_t _fUiParent;
    uint32_t _fExtUiHost;
    uint32_t _fExtUiHostD;
    uint32_t _fDataAccess;
    uint32_t _fWrkSchedule;
    uint32_t _fUiResize;
    uint32_t _fPrgHost;
    uint32_t _fMakePath;
    uint32_t _fMapPath;
    //const LilvNode *_pluginUIType = NULL;
    LV2_URID _uTime_Position;
    LV2_URID _uTime_frame;
    LV2_URID _uTime_speed;
    LV2_URID _uTime_beatsPerMinute;
    LV2_URID _uTime_barBeat;
    LV2_URID _uAtom_EventTransfer;
    bool _hasFreeWheelPort;
    uint32_t _freeWheelPortIndex;
    bool _isConstructed;
    float *_pluginControlsDefault;
    float *_pluginControlsMin;
    float *_pluginControlsMax;
    std::map<QString, LilvNode *> _presets;
public:
    virtual Type synthType() const {
        return LV2_SYNTH;
    }
    LV2Synth ( const QFileInfo &fi, QString label, QString name, QString author, const LilvPlugin *_plugin );
    virtual ~LV2Synth();
    virtual SynthIF *createSIF ( SynthI * );
    bool isSynth() {
        return _isSynth;
    }

    //own public functions
    LV2_URID mapUrid ( const char *uri );
    const char *unmapUrid ( LV2_URID id );
    size_t inPorts() {
        return _audioInPorts.size();
    }
    size_t outPorts() {
        return _audioOutPorts.size();
    }
    bool isConstructed() {return _isConstructed; }
    static void lv2ui_PostShow ( LV2PluginWrapper_State *state );
    static int lv2ui_Resize ( LV2UI_Feature_Handle handle, int width, int height );
    static void lv2ui_Gtk2AllocateCb(int width, int height, void *arg);
    static void lv2ui_Gtk2ResizeCb(int width, int height, void *arg);
    static void lv2ui_ShowNativeGui ( LV2PluginWrapper_State *state, bool bShow );
    static void lv2ui_PortWrite ( LV2UI_Controller controller, uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const *buffer );
    static void lv2ui_Touch (LV2UI_Controller controller, uint32_t port_index, bool grabbed);
    static void lv2ui_ExtUi_Closed ( LV2UI_Controller contr );
    static void lv2ui_SendChangedControls(LV2PluginWrapper_State *state);
    static void lv2state_FillFeatures ( LV2PluginWrapper_State *state );
    static void lv2state_PostInstantiate ( LV2PluginWrapper_State *state );
    static void lv2ui_FreeDescriptors(LV2PluginWrapper_State *state);
    static void lv2state_FreeState(LV2PluginWrapper_State *state);
    static void lv2audio_SendTransport(LV2PluginWrapper_State *state, LV2EvBuf *buffer, LV2EvBuf::LV2_Evbuf_Iterator &iter);
    static void lv2state_InitMidiPorts ( LV2PluginWrapper_State *state );
    static void inline lv2audio_preProcessMidiPorts (LV2PluginWrapper_State *state, unsigned long nsamp, const snd_seq_event_t *events = NULL, unsigned long nevents = 0);
    static void inline lv2audio_postProcessMidiPorts (LV2PluginWrapper_State *state, unsigned long nsamp);
    static const void *lv2state_stateRetreive ( LV2_State_Handle handle, uint32_t key, size_t *size, uint32_t *type, uint32_t *flags );
    static LV2_State_Status lv2state_stateStore ( LV2_State_Handle handle, uint32_t key, const void *value, size_t size, uint32_t type, uint32_t flags );
    static LV2_Worker_Status lv2wrk_scheduleWork(LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data);
    static LV2_Worker_Status lv2wrk_respond(LV2_Worker_Respond_Handle handle, uint32_t size, const void* data);    
    static void lv2conf_write(LV2PluginWrapper_State *state, int level, Xml &xml);
    static void lv2conf_set(LV2PluginWrapper_State *state, const std::vector<QString> & customParams);
    static unsigned lv2ui_IsSupported (const char *, const char *ui_type_uri);
    static void lv2prg_updatePrograms(LV2PluginWrapper_State *state);
    static int lv2_printf(LV2_Log_Handle handle, LV2_URID type, const char *fmt, ...);
    static int lv2_vprintf(LV2_Log_Handle handle, LV2_URID type, const char *fmt, va_list ap);
    static char *lv2state_makePath(LV2_State_Make_Path_Handle handle, const char *path);
    static char *lv2state_abstractPath(LV2_State_Map_Path_Handle handle, const char *absolute_path);
    static char *lv2state_absolutePath(LV2_State_Map_Path_Handle handle, const char *abstract_path);
    static void lv2state_populatePresetsMenu(LV2PluginWrapper_State *state, QMenu *menu);
    static void lv2state_PortWrite ( LV2UI_Controller controller, uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const *buffer, bool fromUi);
    static void lv2state_setPortValue(const char *port_symbol, void *user_data, const void *value, uint32_t size, uint32_t type);
    static void lv2state_applyPreset(LV2PluginWrapper_State *state, LilvNode *preset);
    friend class LV2SynthIF;
    friend class LV2PluginWrapper;
    friend class LV2SynthIF_Timer;


};


class LV2SynthIF : public SynthIF
{
private:
    LV2Synth *_synth;
    LilvInstance *_handle;
    LV2_CONTROL_PORTS _controlInPorts;
    LV2_CONTROL_PORTS _controlOutPorts;
    LV2_AUDIO_PORTS _audioInPorts;
    LV2_AUDIO_PORTS _audioOutPorts;
    LV2_Feature *_ifeatures;
    LV2_Feature **_ppifeatures;
    Port *_controls;
    Port *_controlsOut;
    bool init ( LV2Synth *s );
    size_t _inports;
    size_t _outports;
    size_t _inportsControl;
    size_t _outportsControl;
    size_t _inportsMidi;
    size_t _outportsMidi;    
    float **_audioInBuffers;
    float **_audioOutBuffers;
    float  *_audioInSilenceBuf; // Just all zeros all the time, so we don't have to clear for silence.
    std::vector<unsigned long> _iUsedIdx;  // During process, tells whether an audio input port was used by any input routes.    
    void doSelectProgram(unsigned char channel, int bank, int prog);
    bool processEvent ( const MidiPlayEvent &, snd_seq_event_t * );
    bool lv2MidiControlValues ( size_t port, int ctlnum, int *min, int *max, int *def );
    float midi2Lv2Value ( unsigned long port, int ctlnum, int val );
    LV2PluginWrapper_State *_uiState;    
public:
    LV2SynthIF ( SynthI *s );
    virtual ~LV2SynthIF();

    //virtual methods from SynthIF
    virtual bool initGui();
    virtual void guiHeartBeat();
    virtual bool guiVisible() const;
    virtual void showGui ( bool v );
    virtual bool hasGui() const;
    virtual bool nativeGuiVisible() const;
    virtual void showNativeGui ( bool v );
    virtual bool hasNativeGui() const;
    virtual void getGeometry ( int *, int *, int *, int * ) const;
    virtual void setGeometry ( int, int, int, int );
    virtual void getNativeGeometry ( int *, int *, int *, int * ) const;
    virtual void setNativeGeometry ( int, int, int, int );
    virtual void preProcessAlways();
    virtual iMPEvent getData ( MidiPort *, MPEventList *, iMPEvent, unsigned pos, int ports, unsigned n, float **buffer );
    virtual bool putEvent ( const MidiPlayEvent &ev );
    virtual MidiPlayEvent receiveEvent();
    virtual int eventsPending() const;

    virtual int channels() const;
    virtual int totalOutChannels() const;
    virtual int totalInChannels() const;
    void activate();
    virtual void deactivate();
    virtual void deactivate3();
    virtual QString getPatchName (int, int, bool ) const;
    virtual void populatePatchPopup ( MusEGui::PopupMenu *, int, bool );
    virtual void write ( int level, Xml &xml ) const;
    virtual float getParameter ( unsigned long idx ) const;
    virtual float getParameterOut ( unsigned long n ) const;
    virtual void setParameter ( unsigned long idx, float value );
    virtual int getControllerInfo ( int id, const char **name, int *ctrl, int *min, int *max, int *initval );

    virtual void writeConfiguration ( int level, Xml &xml );
    virtual bool readConfiguration ( Xml &xml, bool readPreset=false );

    virtual void setCustomData ( const std::vector<QString> & );


    unsigned long parameters() const;
    unsigned long parametersOut() const;
    void setParam ( unsigned long i, float val );
    float param ( unsigned long i ) const;
    float paramOut ( unsigned long i ) const;
    const char *paramName ( unsigned long i );
    const char *paramOutName ( unsigned long i );
    virtual CtrlValueType ctrlValueType ( unsigned long ) const;
    virtual CtrlList::Mode ctrlMode ( unsigned long ) const;
    virtual LADSPA_PortRangeHint range(unsigned long i);
    virtual LADSPA_PortRangeHint rangeOut(unsigned long i);

    virtual void enableController(unsigned long i, bool v = true);
    virtual bool controllerEnabled(unsigned long i) const;
    virtual void enableAllControllers(bool v = true);
    virtual void updateControllers();

    void populatePresetsMenu(QMenu *menu);
    void applyPreset(void *preset);


    int id() {
        return MAX_PLUGINS;
    }

    static void lv2prg_Changed(LV2_Programs_Handle handle, int32_t index);

    friend class LV2Synth;
};


class LV2PluginWrapper;
class LV2PluginWrapper_Worker;
class LV2PluginWrapper_Window;

typedef struct _lv2ExtProgram
{
   uint32_t index;
   uint32_t bank;
   uint32_t prog;
   QString name;
   bool useIndex;
   bool operator<(const _lv2ExtProgram& other) const
   {
      if(useIndex == other.useIndex && useIndex == true)
         return index < other.index;

      if(bank < other.bank)
         return true;
      else if(bank == other.bank && prog < other.prog)
         return true;
      return false;
   }

   bool operator==(const _lv2ExtProgram& other) const
   {
      if(useIndex == other.useIndex && useIndex == true)
         return index == other.index;

      return (bank == other.bank && prog == other.prog);
   }


} lv2ExtProgram;

struct LV2PluginWrapper_State {
   LV2PluginWrapper_State():
      _ifeatures(NULL),
      _ppifeatures(NULL),
      widget(NULL),      
      handle(NULL),
      uiDlHandle(NULL),
      uiDesc(NULL),
      uiInst(NULL),
      inst(NULL),
      lastControls(NULL),
      controlsMask(NULL),
      lastControlsOut(NULL),
      plugInst(NULL),
      sif(NULL),
      synth(NULL),
      human_id(NULL),
      iState(NULL),
      tmpValues(NULL),
      numStateValues(0),
      wrkDataSize(0),
      wrkDataBuffer(0),
      wrkThread(NULL),
      wrkEndWork(false),
      controlTimers(NULL),
      deleteLater(false),
      hasGui(false),
      hasExternalGui(false),
      uiIdleIface(NULL),
      uiCurrent(NULL),
      uiX11Size(0, 0),
      pluginWindow(NULL),
      prgIface(NULL),
      uiPrgIface(NULL),
      uiDoSelectPrg(false),
      newPrgIface(false),
      uiChannel(0),
      uiBank(0),
      uiProg(0),
      gtk2Plug(NULL),
      pluginCVPorts(NULL),
      uiControlEvt(128),
      plugControlEvt(128),
      midiEvent(NULL)
   {
      extHost.plugin_human_id = NULL;
      extHost.ui_closed = NULL;
      uiResize.handle = (LV2UI_Feature_Handle)this;
      uiResize.ui_resize = LV2Synth::lv2ui_Resize;
      prgHost.handle = (LV2_Programs_Handle)this;
      prgHost.program_changed = LV2SynthIF::lv2prg_Changed;
      makePath.handle = (LV2_State_Make_Path_Handle)this;
      makePath.path = LV2Synth::lv2state_makePath;
      mapPath.handle = (LV2_State_Map_Path_Handle)this;
      mapPath.absolute_path = LV2Synth::lv2state_absolutePath;
      mapPath.abstract_path = LV2Synth::lv2state_abstractPath;

      midiInPorts.clear();
      midiOutPorts.clear();
      idx2EvtPorts.clear();
   }

    LV2_Feature *_ifeatures;
    LV2_Feature **_ppifeatures;
    void *widget;
    LV2_External_UI_Host extHost;
    LV2_Extension_Data_Feature extData;
    LV2_Worker_Schedule wrkSched;
    LV2_State_Make_Path makePath;
    LV2_State_Map_Path mapPath;
    LilvInstance *handle;
    void *uiDlHandle;
    const LV2UI_Descriptor *uiDesc;
    LV2UI_Handle uiInst;
    LV2PluginWrapper *inst;
    float *lastControls;
    bool *controlsMask;
    float *lastControlsOut;
    PluginI *plugInst;
    LV2SynthIF *sif;
    LV2Synth *synth;
    char *human_id;
    LV2_State_Interface *iState;
    QMap<QString, QPair<QString, QVariant> > iStateValues;
    char **tmpValues;
    size_t numStateValues;
    uint32_t wrkDataSize;
    const void *wrkDataBuffer;
    LV2PluginWrapper_Worker *wrkThread;
    LV2_Worker_Interface *wrkIface;
    bool wrkEndWork;
    int *controlTimers;
    bool deleteLater;
    LV2_Atom_Forge atomForge;
    float curBpm;
    bool curIsPlaying;
    unsigned int curFrame;
    bool hasGui;
    bool hasExternalGui;
    LV2UI_Idle_Interface *uiIdleIface;
    const LilvUI *uiCurrent;    
    LV2UI_Resize uiResize;
    QSize uiX11Size;
    LV2PluginWrapper_Window *pluginWindow;
    LV2_MIDI_PORTS midiInPorts;
    LV2_MIDI_PORTS midiOutPorts;
    LV2_Programs_Interface *prgIface;
    LV2_Programs_UI_Interface *uiPrgIface;
    bool uiDoSelectPrg;
    bool newPrgIface;
    std::map<uint32_t, lv2ExtProgram> index2prg;
    std::map<uint32_t, uint32_t> prg2index;
    LV2_Programs_Host prgHost;
    unsigned char uiChannel;
    int uiBank;
    int uiProg;
    void *gtk2Plug;
    std::map<QString, size_t> controlsNameMap;
    std::map<QString, size_t> controlsSymMap;
    float **pluginCVPorts;
    LV2SimpleRTFifo uiControlEvt;
    LV2SimpleRTFifo plugControlEvt;
    std::map<uint32_t, LV2EvBuf *> idx2EvtPorts;
    snd_midi_event_t *midiEvent;
};


class LV2PluginWrapper_Worker :public QThread
{
private:
    LV2PluginWrapper_State *_state;
    QSemaphore _mSem;
    bool _closing;
public:
    explicit LV2PluginWrapper_Worker ( LV2PluginWrapper_State *s ) : QThread(),
       _state ( s ),
       _mSem(0),
       _closing(false)
    {}

    void run();
    LV2_Worker_Status scheduleWork();
    void makeWork();
    void setClosing() {_closing = true; _mSem.release();}

};



class LV2PluginWrapper_Window : public QMainWindow
{
   Q_OBJECT
protected:
   void closeEvent ( QCloseEvent *event );
private:
   LV2PluginWrapper_State *_state;
   bool _closing;
   QTimer updateTimer;
   void stopUpdateTimer();
public:
   explicit LV2PluginWrapper_Window ( LV2PluginWrapper_State *state );
   void startNextTime();
   void stopNextTime();
   void doChangeControls();
   void setClosing(bool closing) {_closing = closing; }
public slots:
   void updateGui();
};


class LV2PluginWrapper: public Plugin
{
private:
    LV2Synth *_synth;
    std::map<void *, LV2PluginWrapper_State *> _states;
    LADSPA_Descriptor _fakeLd;
    LADSPA_PortDescriptor *_fakePds;       
public:
    LV2PluginWrapper ( LV2Synth *s );
    LV2Synth *synth() {
        return _synth;
    }    
    virtual ~LV2PluginWrapper();
    virtual LADSPA_Handle instantiate ( PluginI * );
    virtual int incReferences ( int ref );
    virtual void activate ( LADSPA_Handle handle );
    virtual void deactivate ( LADSPA_Handle handle );
    virtual void cleanup ( LADSPA_Handle handle );
    virtual void connectPort ( LADSPA_Handle handle, unsigned long port, float *value );
    virtual void apply ( LADSPA_Handle handle, unsigned long n );
    virtual LADSPA_PortDescriptor portd ( unsigned long k ) const;

    virtual LADSPA_PortRangeHint range ( unsigned long i );
    virtual void range ( unsigned long i, float *min, float *max ) const;

    virtual float defaultValue ( unsigned long port ) const;
    virtual const char *portName ( unsigned long i );
    virtual CtrlValueType ctrlValueType ( unsigned long ) const;
    virtual CtrlList::Mode ctrlMode ( unsigned long ) const;
    virtual bool hasNativeGui();
    virtual void showNativeGui ( PluginI *p, bool bShow );
    virtual bool nativeGuiVisible ( PluginI *p );
    virtual void setLastStateControls(LADSPA_Handle handle, size_t index, bool bSetMask, bool bSetVal, bool bMask, float fVal);
    virtual void writeConfiguration(LADSPA_Handle handle, int level, Xml& xml);
    virtual void setCustomData (LADSPA_Handle handle, const std::vector<QString> & customParams);

    void populatePresetsMenu(PluginI *p, QMenu *menu);
    void applyPreset(PluginI *p, void *preset);
};

#endif // LV2_SUPPORT

extern void initLV2();

} // namespace MusECore

#endif



