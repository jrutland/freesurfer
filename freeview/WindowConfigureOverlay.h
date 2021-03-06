/**
 * @file  WindowConfigureOverlay.h
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2015/05/05 18:53:40 $
 *    $Revision: 1.13 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */
#ifndef WINDOWCONFIGUREOVERLAY_H
#define WINDOWCONFIGUREOVERLAY_H

#include <QWidget>
#include "UIUpdateHelper.h"
#include "WidgetHistogram.h"

namespace Ui
{
class WindowConfigureOverlay;
}

class Layer;
class LayerSurface;
class SurfaceLabel;
class SurfaceOverlayProperty;
class QAbstractButton;

class WindowConfigureOverlay : public QWidget, public UIUpdateHelper
{
  Q_OBJECT

public:
  explicit WindowConfigureOverlay(QWidget *parent = 0);
  ~WindowConfigureOverlay();

  virtual void showEvent(QShowEvent *);

signals:
  void ActiveFrameChanged();
  void MaskLoadRequested(const QString& filename);

public slots:
  void UpdateGraph();
  void UpdateUI();
  void OnCurrentVertexChanged();

protected slots:
  void OnActiveSurfaceChanged(Layer* layer);
  void OnClicked( QAbstractButton* btn );
  void OnSliderOpacity( int nVal );
  void OnSpinBoxOpacity( double dVal );
  void OnButtonAdd();
  bool UpdateOverlayProperty( SurfaceOverlayProperty* p );
  void UpdateThresholdChanges();
  void OnHistogramMouseButtonPressed(int button, double value);
  void OnHistogramMarkerChanged();
  void OnSmoothChanged();
  void OnTextThresholdChanged(const QString& strg);
  void OnApply();
  void OnFrameChanged(int nFrame);
  void OnCheckComputeCorrelation(bool bChecked);
  void OnComboCorrelationVolume(int n);
  void OnCheckUsePercentile(bool bChecked);
  void OnCustomColorScale();
  void OnCheckApplyToAll(bool bChecked);
  void OnComboMask(int n);
  void OnCheckInverseMask(bool bChecked);
  void OnSurfaceLabelAdded(SurfaceLabel* label);

private:
  Ui::WindowConfigureOverlay *ui;

  LineMarkers   m_markers;    // custom gradient markers
  LayerSurface* m_layerSurface;
  float*        m_fDataCache;
  double        m_dSavedOffset;
};

#endif // WINDOWCONFIGUREOVERLAY_H
