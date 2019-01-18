""""""""""""""""""""
" General settings "
""""""""""""""""""""
syntax on
set background=light
set t_md=
colorscheme default
"hi Number ctermfg=darkgreen

hi MatchParen cterm=bold ctermbg=none ctermfg=magenta

set expandtab
set shiftround
set showbreak=..
set autochdir

hi TabLineSel ctermfg=Grey ctermbg=Black

let s:tabwidth=4
exec 'set tabstop='.s:tabwidth
exec 'set softtabstop='.s:tabwidth
exec 'set shiftwidth='.s:tabwidth

autocmd FileType c set softtabstop=4 tabstop=4 shiftwidth=4 noexpandtab
autocmd FileType php set softtabstop=2 tabstop=2 shiftwidth=2 noexpandtab

nnoremap ; :
nnoremap : ;

nnoremap j gj
nnoremap k gk

set ai
