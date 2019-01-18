FROM jupyter/datascience-notebook:latest

#RUN conda update  --yes -c conda-forge --update-dependencies --all

RUN conda install --quiet --yes \
    'r-base' \
    'r-irkernel' \
    'r-plyr' \
    'r-devtools' \
    'r-tidyverse' \
    'r-shiny' \
    'r-rmarkdown' \
    'r-forecast' \
    'r-rsqlite' \
    'r-reshape2' \
    'r-caret' \
    'r-rcurl' \
    'r-crayon' \
    'r-randomforest' \
    'r-htmltools' \
    'r-sparklyr' \
    'r-htmlwidgets' \
    'palettable' \
    'seaborn' \
    'pandas' \
    'statsmodels' \
    'r-hexbin' && \
    conda clean -tipsy && \
    fix-permissions $CONDA_DIR

RUN conda install  --quiet --yes -c conda-forge -c ioam ipympl ipywidgets beakerx holoviews nodejs 
#bokeh 

RUN pip install ipywidgets && jupyter nbextension enable --py --sys-prefix widgetsnbextension
RUN pip install ray numexpr bottleneck pyarrow feather-format
USER root
RUN apt-get update && apt-get -yq dist-upgrade \
 && apt-get install -yq --no-install-recommends \
    pypy unzip fontconfig texlive texlive-latex-extra ghostscript dvipng xpdf \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

USER $NB_UID