//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: pcanvas.h,v 1.11.2.4 2009/05/24 21:43:44 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __PCANVAS_H__
#define __PCANVAS_H__

#include "song.h"
#include "canvas.h"
#define beats     4

//---------------------------------------------------------
//   NPart
//    ''visual'' Part
//    wraps Parts with additional information needed
//    for displaying
//---------------------------------------------------------

class NPart : public CItem {
   public:
      NPart(Part* e);
      const QString name() const     { return part()->name(); }
      void setName(const QString& s) { part()->setName(s); }
      Track* track() const           { return part()->track(); }
      };

class QLineEdit;
class MidiEditor;
class QPopupMenu;
class Xml;

//---------------------------------------------------------
//   PartCanvas
//---------------------------------------------------------

class PartCanvas : public Canvas {
      int* _raster;
      TrackList* tracks;

      Part* resizePart;
      QLineEdit* lineEditor;
      NPart* editPart;
      int curColorIndex;
      bool editMode;

      Q_OBJECT
      virtual void keyPress(QKeyEvent*);
      virtual void mousePress(QMouseEvent*);
      virtual void mouseMove(const QPoint&);
      virtual void mouseRelease(const QPoint&);
      virtual void viewMouseDoubleClickEvent(QMouseEvent*);
      virtual void leaveEvent(QEvent*e);
      virtual void drawItem(QPainter&, const CItem*, const QRect&);
      virtual void drawMoving(QPainter&, const CItem*, const QRect&);
      virtual void updateSelection();
      virtual QPoint raster(const QPoint&) const;
      virtual int y2pitch(int y) const;
      virtual int pitch2y(int p) const;
      
      virtual void moveCanvasItems(CItemList&, int, int, DragType, int*);
      // Changed by T356. 
      //virtual bool moveItem(CItem*, const QPoint&, DragType, int*);
      virtual bool moveItem(CItem*, const QPoint&, DragType);
      virtual CItem* newItem(const QPoint&, int);
      virtual void resizeItem(CItem*,bool);
      virtual void newItem(CItem*,bool);
      virtual bool deleteItem(CItem*);
      virtual void startUndo(DragType);
      
      virtual void endUndo(DragType, int);
      virtual void startDrag(CItem*, DragType);
      virtual void dragEnterEvent(QDragEnterEvent*);
      virtual void dragMoveEvent(QDragMoveEvent*);
      virtual void dragLeaveEvent(QDragLeaveEvent*);
      virtual void viewDropEvent(QDropEvent*);

      virtual QPopupMenu* genItemPopup(CItem*);
      virtual void itemPopup(CItem*, int, const QPoint&);

      void glueItem(CItem* item);
      void splitItem(CItem* item, const QPoint&);

      void copy(PartList*);
      void paste(bool clone = false, bool toTrack = true);
      int pasteAt(const QString&, Track*, int, bool clone = false, bool toTrack = true);
      //Part* readClone(Xml&, Track*, bool toTrack = true);
      void drawWavePart(QPainter&, const QRect&, WavePart*, const QRect&);
      Track* y2Track(int) const;
      void drawAudioTrack(QPainter& p, const QRect& r, AudioTrack* track);

   protected:
      virtual void drawCanvas(QPainter&, const QRect&);

   signals:
      void timeChanged(unsigned);
      void tracklistChanged();
      void dclickPart(Track*);
      void selectionChanged();
      void dropSongFile(const QString&);
      void dropMidiFile(const QString&);
      void setUsedTool(int);
      void trackChanged(Track*);
      void selectTrackAbove();
      void selectTrackBelow();

      void startEditor(PartList*, int);

   private slots:
      void returnPressed();

   public:
      enum { CMD_CUT_PART, CMD_COPY_PART, CMD_PASTE_PART, CMD_PASTE_CLONE_PART, CMD_PASTE_PART_TO_TRACK, CMD_PASTE_CLONE_PART_TO_TRACK };

      PartCanvas(int* raster, QWidget* parent, int, int);
      void partsChanged();
      void cmd(int);
   public slots:
   void redirKeypress(QKeyEvent* e) { keyPress(e); }
      };
#endif