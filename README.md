# Zelda3 no PlayStation 2 — revisão 2: 60 FPS fixo

Esta revisão fixa a apresentação em **60 FPS** (sem opção no menu, com frameskip sempre desligado) e elimina a reinicialização longa da fila de áudio durante o jogo. Mantém libpad, vídeo por gsKit, menu em português e preferências junto do save no memory card. Ainda precisa de validação em PS2/PCSX2: a compilação e os testes no computador não comprovam a latência percebida nem a taxa de quadros no console.

## Usar

Para iniciar o ELF separado, mantenha `zelda3_assets.dat` na mesma pasta. Veja [LEIA-ME](ps2/LEIA-ME.txt) para os controles, importação de saves e comportamento sem memory card.

O menu não abre mais na inicialização (o jogo começa direto); abra durante o jogo com **Start + Select**. Direcional/analógico navegam, esquerda/direita alteram valores, X confirma e círculo cancela. As configurações só são aplicadas ao confirmar a saída; salvar é opcional. O arquivo INI externo não é mais necessário nem lido no PS2.

| Botão padrão | Função |
|---|---|
| Direcional / analógico esquerdo | Movimento |
| X | Espada / B |
| Círculo | Ação, correr / A |
| Quadrado | Item / Y |
| Triângulo | Mapa / X |
| Start | Inventário |
| Select | Menu de salvar do jogo |
| L1 / R1 | L / R, com troca de itens opcional |

A apresentação é fixa em **60 FPS**, sem opção no menu: um desenho para cada passo de lógica/áudio. O frameskip também é fixo e fica sempre desligado: nenhum desenho é pulado para recuperar atraso; se o console não acompanhar, o jogo roda mais devagar. Em NTSC, a taxa corresponde a aproximadamente 59,94 apresentações por segundo, sem garantia de desempenho constante no hardware.

Preferências salvas por versões anteriores (por exemplo 30 FPS ou frameskip ligado) são ajustadas sozinhas ao carregar, preservando controles, som e progresso. O INI passa a `Version=2`; não é necessário apagar o memory card.

## Implementação

- Entrada: libpad nas duas portas, negociação analógica assíncrona e limitada, descarte de eixos inválidos de controles digitais, liberação na desconexão, histerese, normalização de direções opostas, ausência de overflow nas diagonais, prioridade do direcional sem apagar o analógico. Saída do menu exige soltar os controles.
- Áudio: o antigo consumidor IOP avançava mesmo sem PCM suficiente. A revisão anterior tratava isso drenando e preenchendo a fila, o que bloqueou a simulação por 250 ms em um caso de teste. O AUDSRV incorporado agora começa vazio, distingue fila cheia de vazia e gera silêncio sem avançar sobre dados ainda não escritos. A EE envia somente dois fragmentos de silêncio inicial; não reinicia nem drena a fila a cada falta de dados. O teto normal é de três passos de áudio, com timeout de 40 ms apenas para uma fila travada.
- Cadência PCM: o conversor de 32 kHz do AUDSRV consome 341 amostras por bloco de 512 amostras da SPU2 a 48 kHz, ou 31.968,75 amostras/s. O orçamento de saída segue essa taxa efetiva, usando `1365364/2560` amostras por passo NTSC. Cada estado de áudio do jogo é sintetizado uma vez e reamostrado nesse orçamento; não há avanço especulativo nem alteração da velocidade ao escolher 30/60 FPS. A síntese SPC/DSP continua na EE; conversão e saída ficam na IOP/SPU2.
- Vídeo: gsKit, GIF/DMA, GS para escala, filtragem e menu. A expansão de paletas fica desligada por padrão e é experimental. Somente 224 linhas são transferidas, reduzindo 12,5% dos bytes de uma textura RGBA por apresentação; a tela inteira não é limpa quando o sprite a cobre. A PPU pode entregar índices de 8 bits ao GS para expansão de paletas quando a linha não exige composição incompatível. Até 32 paletas distintas por quadro; linhas complexas retornam ao caminho RGBA existente. Os buffers de DMA permanecem vivos até a conclusão. O filtro suave usa RGBA para evitar descontinuidades entre paletas.
- Persistência: INI alternado `zelda3.ini` / `zelda3.alt.ini`, SRAM alternada `sram.a` / `sram.b`, com geração, CRC e verificação após a escrita. O banco válido anterior é preservado. `sram.dat` antigo é importado somente se nenhum banco novo for válido. A escolha do cartão considera progresso antes de preferências, e não muda durante a sessão.

A aceleração é **parcial**. A composição de tiles/sprites, diversos efeitos e Mode 7 ainda passam pela CPU. Não foi feita uma reimplementação completa de PPU em GS/VU nem de DSP em SPU2. O modo de paletas nem sempre reduz o custo; deixe desligado para avaliar esta revisão. Não há medição de ganho de desempenho no PS2 nesta entrega.

## Compilar

Requer ps2dev completo, incluindo ps2sdk, gsKit, SDL2 e ps2_drivers. O Makefile também compila o AUDSRV corrigido em `ps2/iop/audsrv` e o incorpora no ELF, substituindo o módulo padrão do arquivo estático de drivers. O fonte e as licenças desse módulo acompanham o pacote. Exportar as variáveis do ambiente instalado:

```sh
export PS2DEV=/caminho/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH"
make -f Makefile.ps2 -j4
mips64r5900el-ps2-elf-strip -o zelda3-ps2-revisado.elf zelda3.elf
python3 -m pip install pycdlib
python3 ps2/make_iso.py zelda3-ps2-revisado.elf zelda3_assets.dat zelda3-ps2-revisado.iso
```

O gerador de ISO verifica o cabeçalho ELF e extrai novamente cada arquivo para comparar seus bytes. Os assets não fazem parte do pacote de fontes.

## Uso de IA

esse projeto teve uso de ia para seu desenvolvimento, mas foi revisado.

## Verificações

[VALIDACAO.md](ps2/VALIDACAO.md) descreve os resultados e seus limites. Testes portáveis:

```sh
./ps2/host-test/run_unit_tests.sh
# Em ambientes que bloqueiam LeakSanitizer/ptrace, manter ASan/UBSan:
ASAN_OPTIONS=detect_leaks=0 ./ps2/host-test/run_unit_tests.sh
```

Para os replays, instale GCC e os arquivos de desenvolvimento SDL2. Use também um executável host compilado a partir da versão original de referência:

```sh
make -C ps2/host-test clean
make -C ps2/host-test EXTRA='-DZ3_GS_TEST=1 -DZ3_FRAME_TEST=1 -DZ3_PIXEL_ABGR=1'
python3 ps2/host-test/compare_replays.py /caminho/original/z3test ps2/host-test/z3test /caminho/zelda3_assets.dat /tmp/z3-30 --fps30
python3 ps2/host-test/compare_replays.py /caminho/original/z3test ps2/host-test/z3test /caminho/zelda3_assets.dat /tmp/z3-60 --palette
```

O primeiro teste confere cada imagem exibida no modo 30 contra o passo correspondente da referência, preservando todas as atualizações da lógica. O segundo exercita também o caminho opcional de paletas. São testes no computador, não benchmarks de PS2.

**Modo de vídeo** (Opções > Vídeo > MODO DE VIDEO): 480i (padrão), 240p ou 480p. O 480p é 704x480 progressivo, configurado como no Metroid Zero Mission DX;
aparece uma tela de confirmação (aperte X em até 10 s para manter, senão volta) e o cartão só guarda o 480p depois de confirmado.

## License

esse projeto foi licenciado com a MIT LICENSE. Cheque 'LICENSE.txt' para mais detalhes.
