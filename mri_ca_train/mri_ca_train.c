/***********************************************************************/
/* mri_ca_train.c                                                      */
/* by Bruce Fischl                                                     */
/*                                                                     */
/* Warning: Do not edit the following four lines.  CVS maintains them. */
/* Revision Author: $Author: tosa $                                           */
/* Revision Date  : $Date: 2003/08/15 13:43:44 $                                             */
/* Revision       : $Revision: 1.16 $                                         */
/***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "mri.h"
#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "utils.h"
#include "timer.h"
#include "gca.h"
#include "transform.h"
#include "cma.h"
#include "flash.h"

int main(int argc, char *argv[]) ;
static int get_option(int argc, char *argv[]) ;
static int replaceLabels(MRI *mri_seg) ;

static int flash = 0 ;
static int binarize = 0 ;
static int binarize_in = 0 ;
static int binarize_out = 0 ;

static int gca_flags = GCA_NO_FLAGS ;

char *Progname ;
static void usage_exit(int code) ;
static char *mask_fname = NULL ;
static char *insert_fname = NULL ;
static int  insert_label = 0 ;
static char *histo_fname = NULL ;

static GCA_PARMS parms ;
static char *seg_dir = "seg" ;
static char T1_name[STRLEN] = "orig" ;
static char *PD_name = NULL ;
static char *xform_name = "talairach.xfm" ;
static int prune = 0 ;
static float smooth = -1 ;
static int gca_inputs = 0 ;
static double TRs[MAX_GCA_INPUTS] ;
static double TEs[MAX_GCA_INPUTS] ;
static double FAs[MAX_GCA_INPUTS] ;
static int map_to_flash = 0 ;

static int ninputs = 1 ;  /* T1 intensity */
static int navgs = 0 ;

static char subjects_dir[STRLEN] ;
static char *heq_fname = NULL ;

static char *input_names[MAX_GCA_INPUTS] = { T1_name } ;
int
main(int argc, char *argv[])
{
  char         **av, fname[STRLEN], *out_fname, *subject_name, *cp ;
  int          ac, nargs, i, n, noint = 0, options ;
  int          msec, minutes, seconds, nsubjects, input, ordering[MAX_GCA_INPUTS], o ;
  struct timeb start ;
  GCA          *gca, *gca_prune = NULL ;
  MRI          *mri_seg, *mri_tmp, *mri_eq = NULL, *mri_inputs ;
  TRANSFORM    *transform ;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  TimerStart(&start) ;

  parms.use_gradient = 0 ;
  parms.node_spacing = 4.0f ;
  parms.prior_spacing = 2.0f ;

  ac = argc ;
  av = argv ;
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (!strlen(subjects_dir)) /* hasn't been set on command line */
  {
    cp = getenv("SUBJECTS_DIR") ;
    if (!cp)
      ErrorExit(ERROR_BADPARM, "%s: SUBJECTS_DIR not defined in environment", 
                Progname);
    strcpy(subjects_dir, cp) ;
    if (argc < 3)
      usage_exit(1) ;
  }


  if (heq_fname)
  {
    mri_eq = MRIread(heq_fname) ;
    if (!mri_eq)
      ErrorExit(ERROR_NOFILE, 
                "%s: could not read histogram equalization volume %s", 
                Progname, heq_fname) ;
  }

  out_fname = argv[argc-1] ;
  nsubjects = argc-2 ;
  for (options = i = 0 ; i < nsubjects ; i++)
  {
    if (argv[i+1][0] == '-')
    {
      nsubjects-- ; options++ ;
    }
  }

  printf("training on %d subject and writing results to %s\n",
         nsubjects, out_fname) ;
  if (gca_inputs == 0)
    gca_inputs = ninputs ;   /* gca reads same # of inputs as we read from command line - not the case if we are mapping to flash */
  n = 0 ;
  if (gca_flags & GCA_GRAD)
  {
    int extra = 0 ;
    if (gca_flags & GCA_XGRAD)
      extra += gca_inputs ;
    if (gca_flags & GCA_YGRAD)
      extra += gca_inputs ;
    if (gca_flags & GCA_ZGRAD)
      extra += gca_inputs ;
    gca_inputs += extra ;
  }
  do
  {
    gca = GCAalloc(gca_inputs, parms.prior_spacing, parms.node_spacing, DEFAULT_VOLUME_SIZE, 
                   DEFAULT_VOLUME_SIZE,DEFAULT_VOLUME_SIZE, gca_flags);

    for (nargs = i = 0 ; i < nsubjects+options ; i++)
    {
      subject_name = argv[i+1] ;
      if (stricmp(subject_name, "-NOINT") == 0)
      {
        printf("not using intensity information for subsequent subjects...\n");
        noint = 1 ; nargs++ ;
        continue ;
      }
      else if (stricmp(subject_name, "-INT") == 0)
      {
        printf("using intensity information for subsequent subjects...\n");
        noint = 0 ; nargs++ ;
        continue ;
      }
      sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name, seg_dir) ;
      if (DIAG_VERBOSE_ON)
        printf("reading segmentation from %s...\n", fname) ;
      mri_seg = MRIread(fname) ;
      if (!mri_seg)
        ErrorExit(ERROR_NOFILE, "%s: could not read segmentation file %s",
                  Progname, fname) ;
      if (binarize)
      {
        int i ;
        for (i = 0 ; i < 256 ; i++)
        {
          if (i == binarize_in)
            MRIreplaceValues(mri_seg, mri_seg, i, binarize_out) ;
          else
            MRIreplaceValues(mri_seg, mri_seg, i, 0) ;
        }
      }
      if (insert_fname)
      {
        MRI *mri_insert ;

        sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name, insert_fname) ;
        mri_insert = MRIread(fname) ;
        if (mri_insert == NULL)
          ErrorExit(ERROR_NOFILE, "%s: could not read volume from %s for insertion",
                    Progname, insert_fname) ;

        MRIbinarize(mri_insert, mri_insert, 1, 0, insert_label) ;
        MRIcopyLabel(mri_insert, mri_seg, insert_label) ;
        MRIfree(&mri_insert) ;
      }
        
      replaceLabels(mri_seg) ;
      MRIeraseBorderPlanes(mri_seg) ;

      if (i != 0)  /* not the first image read - reorder it to be in the same order as 1st */
      {
        for (input = 0 ; input < ninputs ; input++)
        { 
          sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name,input_names[input]);
          mri_tmp = MRIreadInfo(fname) ;
          if (!mri_tmp)
            ErrorExit(ERROR_NOFILE, "%s: could not read image from file %s", Progname, fname) ;
          for (o = 0 ; o < ninputs ; o++)
            if (FEQUAL(TRs[o],mri_tmp->tr) && FEQUAL(FAs[o],mri_tmp->flip_angle) && FEQUAL(TEs[o], mri_tmp->te))
              ordering[input] = o ;
          MRIfree(&mri_tmp) ;
        }
      }
      else
        for (o = 0 ; o < ninputs ; o++)
          ordering[o] = o ;

      if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      {
        printf("ordering images: ") ;
        for (o = 0 ; o < ninputs ; o++)
          printf("%d ", ordering[o]) ;
        printf("\n") ;
      }
      
      for (input = 0 ; input < ninputs ; input++)
      {      
        sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name,input_names[ordering[input]]);
        if (DIAG_VERBOSE_ON)
          printf("reading co-registered input from %s...\n", fname) ;
        mri_tmp = MRIread(fname) ;
        if (!mri_tmp)
          ErrorExit(ERROR_NOFILE, "%s: could not read image from file %s", Progname, fname) ;
        if (i == 0)
        {
          TRs[input] = mri_tmp->tr ;
          FAs[input] = mri_tmp->flip_angle ;
          TEs[input] = mri_tmp->te ;
        }
        else if (!FEQUAL(TRs[input],mri_tmp->tr) || !FEQUAL(FAs[input],mri_tmp->flip_angle) ||
                 !FEQUAL(TEs[input], mri_tmp->te))
          ErrorExit(ERROR_BADPARM, "%s: subject %s input volume %s: sequence parameters (%2.1f, %2.1f, %2.1f)"
                    "don't match other inputs (%2.1f, %2.1f, %2.1f)",
                    Progname, subject_name, fname, 
                    mri_tmp->tr, DEGREES(mri_tmp->flip_angle), mri_tmp->te,
                    TRs[input], DEGREES(FAs[input]), TEs[input]) ;
        
        if (input == 0)
        {
          int nframes = ninputs ;
          
          if (gca_flags & GCA_XGRAD)
            nframes += ninputs ;
          if (gca_flags & GCA_YGRAD)
            nframes += ninputs ;
          if (gca_flags & GCA_ZGRAD)
            nframes += ninputs ;
          mri_inputs = 
            MRIallocSequence(mri_tmp->width, mri_tmp->height, mri_tmp->depth,
                             mri_tmp->type, nframes) ;
          if (!mri_inputs)
            ErrorExit(ERROR_NOMEMORY, 
                      "%s: could not allocate input volume %dx%dx%dx%d",
                      mri_tmp->width, mri_tmp->height, mri_tmp->depth,nframes) ;
          MRIcopyHeader(mri_tmp, mri_inputs) ;
        }
        
        if (mask_fname)
        {
          MRI *mri_mask ;
          
          sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name, mask_fname);
          printf("reading volume %s for masking...\n", fname) ;
          mri_mask = MRIread(fname) ;
          if (!mri_mask)
            ErrorExit(ERROR_NOFILE, "%s: could not open mask volume %s.\n",
                      Progname, fname) ;
          
          MRImask(mri_tmp, mri_mask, mri_tmp, 0, 0) ;
          MRIfree(&mri_mask) ;
        }
        if (mri_eq && !noint)
        {
          printf("histogram equalizing input image...\n") ;
          MRIhistoEqualize(mri_tmp, mri_eq, mri_tmp, 30, 170) ;
        }
        MRIcopyFrame(mri_tmp, mri_inputs, 0, input) ;
        MRIfree(&mri_tmp) ;
      }
      if (i == 0 && flash)   /* first subject */
        GCAsetFlashParameters(gca, TRs, FAs, TEs) ;
      printf("processing subject %s, %d of %d...\n", subject_name,i+1-nargs,
             nsubjects);
      
      if (xform_name)
      {
        sprintf(fname, "%s/%s/mri/transforms/%s", 
                subjects_dir, subject_name, xform_name) ;
        if (DIAG_VERBOSE_ON)
          printf("reading transform from %s...\n", fname) ;
        transform = TransformRead(fname) ;
        if (!transform)
          ErrorExit(ERROR_NOFILE, "%s: could not read transform from file %s",
                    Progname, fname) ;
        TransformInvert(transform, mri_inputs) ;
      }
      else
        transform = TransformAlloc(LINEAR_VOXEL_TO_VOXEL, NULL) ;

      if (map_to_flash)
      {
        MRI *mri_tmp ;
        
        mri_tmp = MRIparameterMapsToFlash(mri_inputs, NULL, TRs, TEs, FAs, gca_inputs) ;
        MRIfree(&mri_inputs) ;
        mri_inputs = mri_tmp ;
      }
      
      if (gca_flags & GCA_GRAD)
      {
        MRI *mri_kernel, *mri_smooth, *mri_grad, *mri_tmp ;
        int i, start ;
        
        mri_kernel = MRIgaussian1d(1.0, 30) ;
        mri_smooth = MRIconvolveGaussian(mri_inputs, NULL, mri_kernel) ;
        
        if (mri_inputs->type != MRI_FLOAT)
        {
          mri_tmp = MRISeqchangeType(mri_inputs, MRI_FLOAT, 0, 0, 1) ;
          MRIfree(&mri_inputs) ; mri_inputs = mri_tmp ;
        }
        
        start = ninputs ;
        if (gca_flags & GCA_XGRAD)
        {
          for (i = 0 ; i < ninputs ; i++)
          {
            mri_grad = MRIxSobel(mri_smooth, NULL, i) ;
            MRIcopyFrame(mri_grad, mri_inputs, 0, start+i) ;
            MRIfree(&mri_grad) ;
          }
          start += ninputs ;
        }
        if (gca_flags & GCA_YGRAD)
        {
          for (i = 0 ; i < ninputs ; i++)
          {
            mri_grad = MRIySobel(mri_smooth, NULL, i) ;
            MRIcopyFrame(mri_grad, mri_inputs, 0, start+i) ;
            MRIfree(&mri_grad) ;
          }
          start += ninputs ;
        }
        if (gca_flags & GCA_ZGRAD)
        {
          for (i = 0 ; i < ninputs ; i++)
          {
            mri_grad = MRIzSobel(mri_smooth, NULL, i) ;
            MRIcopyFrame(mri_grad, mri_inputs, 0, start+i) ;
            MRIfree(&mri_grad) ;
          }
          start += ninputs ;
        }
        
        MRIfree(&mri_kernel) ; MRIfree(&mri_smooth) ; 
      }
      GCAtrain(gca, mri_inputs, mri_seg, transform, gca_prune, noint) ;
      MRIfree(&mri_seg) ; MRIfree(&mri_inputs) ; TransformFree(&transform) ;
    }
    
    GCAcompleteMeanTraining(gca) ;
    
    /* now compute covariances */
    for (nargs = i = 0 ; i < nsubjects+options ; i++)
    {
      subject_name = argv[i+1] ;
      if (stricmp(subject_name, "-NOINT") == 0)
      {
        printf("not using intensity information for subsequent subjects...\n");
        noint = 1 ; nargs++ ;
        continue ;
      }
      else if (stricmp(subject_name, "-INT") == 0)
      {
        printf("using intensity information for subsequent subjects...\n");
        noint = 0 ; nargs++ ;
        continue ;
      }
      if (noint)
      {
        printf("skipping covariance calculation for subject %s...\n", subject_name) ;
        continue ;
      }
      printf("computing covariances for subject %s, %d of %d...\n", subject_name,i+1-nargs,
             nsubjects);
      sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name, seg_dir) ;
      if (DIAG_VERBOSE_ON)
        printf("reading segmentation from %s...\n", fname) ;
      mri_seg = MRIread(fname) ;
      if (!mri_seg)
        ErrorExit(ERROR_NOFILE, "%s: could not read segmentation file %s",
                  Progname, fname) ;
      if (insert_fname)
      {
        MRI *mri_insert ;

        sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name, insert_fname) ;
        mri_insert = MRIread(fname) ;
        if (mri_insert == NULL)
          ErrorExit(ERROR_NOFILE, "%s: could not read volume from %s for insertion",
                    Progname, insert_fname) ;

        MRIbinarize(mri_insert, mri_insert, 1, 0, insert_label) ;
        MRIcopyLabel(mri_insert, mri_seg, insert_label) ;
        MRIfree(&mri_insert) ;
      }
        
      replaceLabels(mri_seg) ;
      MRIeraseBorderPlanes(mri_seg) ;
      
      for (input = 0 ; input < ninputs ; input++)
      {      
        sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name,input_names[input]);
        if (DIAG_VERBOSE_ON)
          printf("reading co-registered input from %s...\n", fname) ;
        mri_tmp = MRIread(fname) ;
        if (!mri_tmp)
          ErrorExit(ERROR_NOFILE, "%s: could not read T1 data from file %s",
                    Progname, fname) ;
        
        if (input == 0)
        {
          int nframes = ninputs ;
          
          if (gca_flags & GCA_XGRAD)
            nframes += ninputs ;
          if (gca_flags & GCA_YGRAD)
            nframes += ninputs ;
          if (gca_flags & GCA_ZGRAD)
            nframes += ninputs ;
          mri_inputs = 
            MRIallocSequence(mri_tmp->width, mri_tmp->height, mri_tmp->depth,
                             mri_tmp->type, nframes) ;
          if (!mri_inputs)
            ErrorExit(ERROR_NOMEMORY, 
                      "%s: could not allocate input volume %dx%dx%dx%d",
                      mri_tmp->width, mri_tmp->height, mri_tmp->depth,ninputs) ;
          MRIcopyHeader(mri_tmp, mri_inputs) ;
        }
        
        if (mask_fname)
        {
          MRI *mri_mask ;
          
          sprintf(fname, "%s/%s/mri/%s", subjects_dir, subject_name, mask_fname);
          printf("reading volume %s for masking...\n", fname) ;
          mri_mask = MRIread(fname) ;
          if (!mri_mask)
            ErrorExit(ERROR_NOFILE, "%s: could not open mask volume %s.\n",
                      Progname, fname) ;
          
          MRImask(mri_tmp, mri_mask, mri_tmp, 0, 0) ;
          MRIfree(&mri_mask) ;
        }
        if (mri_eq && !noint)
        {
          printf("histogram equalizing input image...\n") ;
          MRIhistoEqualize(mri_tmp, mri_eq, mri_tmp, 30, 170) ;
        }
        MRIcopyFrame(mri_tmp, mri_inputs, 0, input) ;
        MRIfree(&mri_tmp) ;
      }
      
      if (xform_name)
      {
        sprintf(fname, "%s/%s/mri/transforms/%s", 
                subjects_dir, subject_name, xform_name) ;
        if (DIAG_VERBOSE_ON)
          printf("reading transform from %s...\n", fname) ;
        transform = TransformRead(fname) ;
        if (!transform)
          ErrorExit(ERROR_NOFILE, "%s: could not read transform from file %s",
                    Progname, fname) ;
        TransformInvert(transform, mri_inputs) ;
      }
      else
        transform = TransformAlloc(LINEAR_VOXEL_TO_VOXEL, NULL) ;
      
      if (map_to_flash)
      {
        MRI *mri_tmp ;
        
        mri_tmp = MRIparameterMapsToFlash(mri_inputs, NULL, TRs, TEs, FAs, gca_inputs) ;
        MRIfree(&mri_inputs) ;
        mri_inputs = mri_tmp ;
      }
      
      if (gca_flags & GCA_GRAD)
      {
        MRI *mri_kernel, *mri_smooth, *mri_grad, *mri_tmp ;
        int i, start ;
        
        mri_kernel = MRIgaussian1d(1.0, 30) ;
        mri_smooth = MRIconvolveGaussian(mri_inputs, NULL, mri_kernel) ;
        
        if (mri_inputs->type != MRI_FLOAT)
        {
          mri_tmp = MRISeqchangeType(mri_inputs, MRI_FLOAT, 0, 0, 1) ;
          MRIfree(&mri_inputs) ; mri_inputs = mri_tmp ;
        }
        
        start = ninputs ;
        if (gca_flags & GCA_XGRAD)
        {
          for (i = 0 ; i < ninputs ; i++)
          {
            mri_grad = MRIxSobel(mri_smooth, NULL, i) ;
            MRIcopyFrame(mri_grad, mri_inputs, 0, start+i) ;
            MRIfree(&mri_grad) ;
          }
          start += ninputs ;
        }
        if (gca_flags & GCA_YGRAD)
        {
          for (i = 0 ; i < ninputs ; i++)
          {
            mri_grad = MRIySobel(mri_smooth, NULL, i) ;
            MRIcopyFrame(mri_grad, mri_inputs, 0, start+i) ;
            MRIfree(&mri_grad) ;
          }
          start += ninputs ;
        }
        if (gca_flags & GCA_ZGRAD)
        {
          for (i = 0 ; i < ninputs ; i++)
          {
            mri_grad = MRIzSobel(mri_smooth, NULL, i) ;
            MRIcopyFrame(mri_grad, mri_inputs, 0, start+i) ;
            MRIfree(&mri_grad) ;
          }
          start += ninputs ;
        }
        
        MRIfree(&mri_kernel) ; MRIfree(&mri_smooth) ; 
      }
      GCAtrainCovariances(gca, mri_inputs, mri_seg, transform) ;
      MRIfree(&mri_seg) ; MRIfree(&mri_inputs) ; TransformFree(&transform) ;
    }
    
    GCAcompleteCovarianceTraining(gca) ;
    if (gca_prune)
      GCAfree(&gca_prune) ;
    gca_prune = gca ;
  } while (n++ < prune) ;

  if (smooth > 0)
  {
    printf("regularizing conditional densities with smooth=%2.2f\n", smooth) ;
    GCAregularizeConditionalDensities(gca, smooth) ;
  }
  if (navgs)
  {
    printf("applying mean filter %d times to conditional densities\n", navgs) ;
    GCAmeanFilterConditionalDensities(gca, navgs) ;
  }

  printf("writing trained GCA to %s...\n", out_fname) ;
  if (GCAwrite(gca, out_fname) != NO_ERROR)
    ErrorExit(ERROR_BADFILE, "%s: could not write gca to %s", Progname, out_fname) ;

  if (histo_fname)
  {
    FILE *fp ;
    int   histo_counts[10000], xn, yn, zn, max_count ;
    GCA_NODE  *gcan ;

    memset(histo_counts, 0, sizeof(histo_counts)) ;
    fp = fopen(histo_fname, "w") ;
    if (!fp)
      ErrorExit(ERROR_BADFILE, "%s: could not open histo file %s",
                Progname, histo_fname) ;

    max_count = 0 ;
    for (xn = 0 ; xn < gca->node_width;  xn++)
    {
      for (yn = 0 ; yn < gca->node_height ; yn++)
      {
        for (zn = 0 ; zn < gca->node_depth ; zn++)
        {
          gcan = &gca->nodes[xn][yn][zn] ;
          if (gcan->nlabels < 1)
            continue ;
          if (gcan->nlabels == 1 && IS_UNKNOWN(gcan->labels[0]))
            continue ;
          histo_counts[gcan->nlabels]++ ;
          if (gcan->nlabels > max_count)
            max_count = gcan->nlabels ;
        }
      }
    }
    max_count = 20 ;
    for (xn = 1 ; xn < max_count ;  xn++)
      fprintf(fp, "%d %d\n", xn, histo_counts[xn]) ;
    fclose(fp) ;
  }

  GCAfree(&gca) ;
  msec = TimerStop(&start) ;
  seconds = nint((float)msec/1000.0f) ;
  minutes = seconds / 60 ;
  seconds = seconds % 60 ;
  printf("classifier array training took %d minutes"
          " and %d seconds.\n", minutes, seconds) ;
  exit(0) ;
  return(0) ;
}
/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
        static int first_input = 1 ;
  int  nargs = 0 ;
  char *option ;
  
  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "GRADIENT"))
  {
    parms.use_gradient = 1 ;
    ninputs += 3 ;  /* components of the gradient */
  }
  else if (!stricmp(option, "PRIOR_SPACING"))
  {
    parms.prior_spacing = atof(argv[2]) ;
    nargs = 1 ;
    printf("spacing priors every %2.1f mm\n", parms.prior_spacing) ;
  }
  else if (!stricmp(option, "XGRAD"))
  {
    gca_flags |= GCA_XGRAD ;
    printf("using x gradient information in training...\n") ;
  }
  else if (!stricmp(option, "YGRAD"))
  {
    gca_flags |= GCA_YGRAD ;
    printf("using y gradient information in training...\n") ;
  }
  else if (!stricmp(option, "ZGRAD"))
  {
    gca_flags |= GCA_ZGRAD ;
    printf("using z gradient information in training...\n") ;
  }
  else if (!stricmp(option, "FLASH"))
  {
#if 1
    flash = 0 ;
    printf("setting gca->type to FLASH\n") ;
#else
    int i ;
    
    map_to_flash = 1 ;
    gca_inputs = atoi(argv[2]) ;
    nargs = 1+3*gca_inputs ;
    printf("mapping T1/PD inputs to flash volumes:\n") ;
    for (i = 0 ; i < gca_inputs ; i++)
    {
      TRs[i] = atof(argv[3+3*i]) ;
      FAs[i] = RADIANS(atof(argv[4+3*i])) ;
      TEs[i] = atof(argv[5+3*i]) ;
      printf("\tvolume %d: TR=%2.1f msec, flip angle %2.1f, TE=%2.1f msec\n",
             i, TRs[i], DEGREES(FAs[i]), TEs[i]) ;
    }
#endif
  }
  else if (!stricmp(option, "INPUT"))
  {
    if (first_input)
    {
      ninputs-- ;
      first_input = 0 ;
    }
    
    input_names[ninputs++] = argv[2] ;
    nargs = 1 ;
    printf("input[%d] = %s\n", ninputs-1, input_names[ninputs-1]) ;
  }
  else if (!stricmp(option, "NODE_SPACING"))
  {
    parms.node_spacing = atof(argv[2]) ;
    nargs = 1 ;
    printf("spacing nodes every %2.1f mm\n", parms.node_spacing) ;
  }
  else if (!stricmp(option, "BINARIZE"))
  {
    binarize = 1 ; 
    binarize_in = atoi(argv[2]) ;
    binarize_out = atoi(argv[3]) ;
    nargs = 2 ;
    printf("binarizing segmentation values, setting input %d to output %d\n",
           binarize_in, binarize_out) ;
  }
  else if (!stricmp(option, "NOMRF"))
  {
    gca_flags |= GCA_NO_MRF ;
    printf("not computing MRF statistics...\n") ;
  }
  else if (!stricmp(option, "MASK"))
  {
    mask_fname = argv[2] ;
    nargs = 1 ;
    printf("using MR volume %s to mask input volume...\n", mask_fname) ;
  }
  else if (!stricmp(option, "DEBUG_NODE"))
  {
    Ggca_x = atoi(argv[2]) ;
    Ggca_y = atoi(argv[3]) ;
    Ggca_z = atoi(argv[4]) ;
    nargs = 3 ;
    printf("debugging node (%d, %d, %d)\n", Ggca_x,Ggca_y,Ggca_z) ;
  }
  else if (!stricmp(option, "DEBUG_VOXEL"))
  {
    Gx = atoi(argv[2]) ;
    Gy = atoi(argv[3]) ;
    Gz = atoi(argv[4]) ;
    nargs = 3 ;
    printf("debugging voxel (%d, %d, %d)\n", Gx,Gy,Gz) ;
  }
  else if (!stricmp(option, "DEBUG_LABEL"))
  {
    Ggca_label = atoi(argv[2]) ;
    nargs = 1 ;
    printf("debugging label %d\n", Ggca_label) ;
  }
  else if (!stricmp(option, "INSERT"))
  {
    insert_fname = argv[2] ;
    insert_label = atoi(argv[3]) ;
    nargs = 2 ;
    printf("inserting non-zero vals from %s as label %d...\n", insert_fname,insert_label);
  }
  else if (!stricmp(option, "PRUNE"))
  {
    prune = atoi(argv[2]) ;
    nargs = 1 ;
    printf("pruning classifier %d times after initial training\n", prune) ;
  }
  else if (!stricmp(option, "HEQ"))
  {
    heq_fname = argv[2] ;
    nargs = 1 ;
    printf("reading template for histogram equalization from %s...\n", 
           heq_fname) ;
  }
  else if (!stricmp(option, "T1"))
  {
    strcpy(T1_name, argv[2]) ;
    nargs = 1 ;
    printf("reading T1 data from subject's mri/%s directory\n",
            T1_name) ;
  }
  else if (!stricmp(option, "PD"))
  {
    PD_name = argv[2] ;
    nargs = 1 ;
    printf("reading PD data  subject's mri/%s file\n",PD_name) ;
  }
  else if (!stricmp(option, "PARC_DIR") || !stricmp(option, "SEG_DIR") || 
                                         !stricmp(option, "SEGMENTATION"))
  {
    seg_dir = argv[2] ;
    nargs = 1 ;
    printf("reading segmentation from subject's mri/%s directory\n",
            seg_dir) ;
  }
  else if (!stricmp(option, "XFORM"))
  {
    xform_name = argv[2] ;
    nargs = 1 ;
    printf("reading xform from %s\n", xform_name) ;
  }
  else if (!stricmp(option, "NOXFORM"))
  {
    xform_name = NULL ;
    printf("disabling application of xform...\n") ;
  }
  else if (!stricmp(option, "SDIR"))
  {
    strcpy(subjects_dir, argv[2]) ;
    nargs = 1 ;
    printf("using %s as subjects directory\n", subjects_dir) ;
  }
  else if (!stricmp(option, "SMOOTH"))
  {
    smooth = atof(argv[2]) ;
    if (smooth <= 0 || smooth > 1)
      ErrorExit(ERROR_BADPARM, 
                "%s: smoothing parameter %2.1f must be in [0,1]\n",
                Progname, smooth) ;
    nargs = 1 ;
    printf("imposing %2.1f smoothing on conditional statistics\n", smooth) ;
  }
  else switch (toupper(*option))
  {
  case 'A':
    navgs = atoi(argv[2]) ;
    printf("applying %d mean filters to classifiers after training\n",navgs);
    nargs = 1 ;
    break ;
  case 'H':
    histo_fname = argv[2] ;
    nargs = 1 ;
    printf("writing histogram of classes/voxel to %s\n", histo_fname) ;
    break; 
  case '?':
  case 'U':
    usage_exit(0) ;
    break ;
  default:
    fprintf(stderr, "unknown option %s\n", argv[1]) ;
    exit(1) ;
    break ;
  }

  return(nargs) ;
}
/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static void
usage_exit(int code)
{
  printf("usage: %s [options] <subject 1> <subject 2> ... <output file>\n",
         Progname) ;
  printf(
         "\t-node_spacing   - spacing of classifiers in canonical space\n"
         "\t-prior_spacing  - spacing of class priors in canonical space\n");
  exit(code) ;
}
static int input_labels[] = {  
  Left_Cerebral_Exterior,
  Right_Cerebral_Exterior,
  Left_Cerebellum_Exterior,
  Right_Cerebellum_Exterior
} ;
static int output_labels[] = {  
  Left_Cerebral_Cortex,
  Right_Cerebral_Cortex,
  Left_Cerebellum_Cortex,
  Right_Cerebellum_Cortex
} ;
  
static int
replaceLabels(MRI *mri_seg)
{
  int    i ;

  for (i = 0 ; i < sizeof(output_labels)/sizeof(output_labels[0]) ; i++)
    MRIreplaceValues(mri_seg, mri_seg, input_labels[i], output_labels[i]) ;
  return(NO_ERROR) ;
}

