# Validação R5 — 15/11/2026

## O problema e a correção

Na R4, uma apresentação corresponde a uma atualização da lógica. Portanto, uma queda real de 60 para 30 apresentações/s desacelera o jogo. O menu e o seletor 30/60 continuam fora do executável. Não foi acrescentado frameskip.

O percurso inicial mostra dois custos desnecessários: desenhar a subtela inteira quando nenhum pixel visível habilita sua mistura e repetir a mistura de cores em cada pixel. A R5 verifica a necessidade da subtela, reutiliza paletas por cor secundária e evita invalidá-las quando as cores e o brilho não mudaram. Um índice de paleta escuro não é considerado equivalente a outro só porque seu RGB arredondado coincide: a comparação do cache usa as cores de origem.

## Medição no computador

| Caso | Amostra | PPU R4 | PPU R5 | Redução de tempo |
|---|---:|---:|---:|---:|
| Castelo com subtela | 9.620 quadros por execução | 215,627 µs | 145,992 µs | 32,3% |
| Chuva | 1.891 quadros por execução | 290,742 µs | 214,415 µs | 26,3% |
| Interior sem subtela, controle | 4.734 quadros por execução | 124,3745 µs | 123,488 µs | 0,7% |
| Sala escura com iluminação por subtela | 2.463 quadros | 284,389 µs | 177,318 µs | 37,6% |
| Sala escura sem subtela | 4.323 quadros | 129,166 µs | 127,943 µs | 0,9% |

Castelo, chuva e controle: mediana de três execuções sequenciais por versão, alternando a ordem, cada uma reproduzindo 24.000 quadros. Escuridão: uma execução por versão de 40.000 quadros; seleção pelos indicadores reais de sala escura, módulo, submódulo e subtela. Salas: 33, 34, 50, 65 e 66 (decimal).

Os tempos vêm do trecho `ZeldaDrawPpuFrame` no host x86-64, com GCC `-O1` e sanitizer de alinhamento. Não incluem lógica, áudio nem apresentação SDL. Não são FPS e **não preveem o ganho do PS2**, que usa R5900, caches, DMA, GS e compilação `-O2` diferentes. As diferenças de menos de 1% no controle e na escuridão sem subtela não demonstram ganho relevante. O resultado escuro agregado também é dominado pelos trechos sem subtela; os 37,6% se referem apenas ao caminho de iluminação indicado na tabela.

## Correção visual e funcionamento

- 40.000 quadros do percurso inicial: mesmos hashes de pixels da R4, cobrindo chuva, castelo, escuridão/lanterna e transições. Capturas internas dos trechos foram inspecionadas.
- Outros 35.000 quadros dos 14 casos de regressão (introdução e 13 capítulos): hashes iguais à R4, sem erros de alinhamento.
- 35.000 quadros PT-BR: sem crash/erro de alinhamento, idioma estendido `3` selecionado. Replays ingleses não cobrem todas as rotas da tradução.
- 3.072 quadros sintéticos de PPU sob ASan/alignment: janelas, soma/subtração, meia cor, saturação, brilho, sprites nos oito pares de tamanho, limites ligados/desligados, mosaico, Mode 7, escritas de OAM/CGRAM no meio do quadro. Inclui cache aquecido, alterações diretas de paleta/brilho e wrap da geração do cache. Pixels iguais à R4.
- Suíte portável ASan/UBSan: controles, seleção de cartão, SRAM e preferências, fila de áudio, 120 mil quadros simulados mono/estéreo, fase exata de reamostragem, posse de buffers SIF e erros de saída. O áudio não sofreu alterações na R5.
- O ELF foi compilado/linkado para PS2; a IOP embutida permanece a da R4. O menu não é ligado. Inspeção da função nativa principal confirma ausência de `SDL_Delay` e presença das chamadas de lógica, áudio e desenho.
- A ISO foi reaberta e seus arquivos comparados byte a byte com ELF, assets, SYSTEM.CNF e instruções de origem.

LeakSanitizer foi desativado por restrição do ambiente; ASan e UBSan permaneceram ativos nos testes portáveis. Os testes antigos do menu e da política 30/60 permanecem como compatibilidade do fonte, sem entrar no fluxo do ELF.

## Medição no PS2

L3 alterna um painel opcional. FPS conta apresentações reais. J = lógica; A = áudio; P = PPU; GS = envio/desenho; V = espera de VBlank. Valores são médias por quadro em milissegundos, atualizadas aproximadamente a cada segundo. O painel não grava configurações e seus próprios comandos de desenho entram em GS. Os timers por etapa não são consultados enquanto o painel está oculto.

**A R5 não foi executada aqui em PS2 nem PCSX2.** O painel foi compilado, mas a leitura real e a estabilidade de 60 FPS nessas cenas ainda precisam de validação no aparelho. O tempo de VBlank pode quantizar a saída em cerca de 30 FPS quando o trabalho ultrapassa o orçamento de um campo; isso é uma explicação compatível com o relato, não uma medição feita neste ambiente.

A PPU e o DSP continuam parcialmente na EE. Esta revisão não afirma portar toda a PPU para o GS. Não altera saves, ROM/assets ou o site do extrator.

## Reproduzir

```sh
ASAN_OPTIONS=detect_leaks=0 sh ps2/host-test/run_unit_tests.sh
ASAN_OPTIONS=detect_leaks=0 sh ps2/host-test/compare_ppu.sh /caminho/fonte-r4
make -C ps2/host-test EXTRA='-DZ3_GS_TEST=1 -DZ3_PIXEL_ABGR=1'
python3 ps2/host-test/compare_replays.py /caminho/r4/z3test ps2/host-test/z3test /caminho/zelda3_assets.dat /tmp/z3-r5
```

Em uma pasta contendo o DAT, `zelda3.ini` de teste com áudio desativado e `saves/ref`, o teste de efeitos pode ser reproduzido com:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy Z3_REPLAY=0 Z3_FRAMES=40000 Z3_PROFILE=profile.csv Z3_HASH=frames.hash /caminho/z3test
```

Para medir a R4, gere seu `main_test.c` com os mesmos hooks de perfil da R5 (o gerador aceita o caminho de `src/main.c` e o destino), recompile e rode nas mesmas condições. CSV contém módulo, sala, subtela, cor e indicador de escuridão. Os perfis são de CPU do host, não do PS2.

Resultados desta revisão em `ps2/host-test/results-r5/`. Pastas `results/` e `results-r4/` são históricas.
