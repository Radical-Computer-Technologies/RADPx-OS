" RADix OS Vim defaults.
"
" Keep this file self-contained until the full upstream Vim runtime is staged.

if exists('skip_defaults_vim')
  finish
endif

if &compatible
  set nocompatible
endif

set ttimeout
set ttimeoutlen=100
set display=truncate
set scrolloff=3
set backspace=indent,eol,start
set ruler
set showcmd
set laststatus=1
set noerrorbells
set novisualbell
set nomodeline

" Avoid sourcing filetype/plugin runtime files until they are packaged.
filetype off
