# Zelda3 no PlayStation 2 — R5, 15/11/2026

Esta versão abre diretamente o jogo, com alvo de 60 FPS, sem frameskip e sem menu de configurações. O atalho L2 + R2 + Start foi retirado. O inventário e a tela de salvar do Zelda permanecem. Preferências antigas não ativam 30 FPS nesta versão; saves e outras configurações existentes são preservados, sem regravar o INI.

Veja [LEIA-ME](ps2/LEIA-ME.txt) para instalação e controles. A ISO e o ZIP PT-BR incluem os assets extraídos da ROM fornecida. O ELF separado precisa do DAT ao lado. A compatibilidade PT-BR da R3 foi mantida.

## Correção das cenas pesadas na R5

A R4 fazia uma atualização da lógica por apresentação. Assim, cair para 30 FPS também desacelera o jogo. Não existe mais seleção automática de 30: o custo do desenho pode ultrapassar o intervalo de VBlank e levar a apresentar a cada dois intervalos.

O percurso inicial foi reproduzido por 40 mil quadros, cobrindo chuva, castelo e salas escuras com lanterna. O caminho de subtela/mistura na CPU executava trabalho desnecessário: dentro do castelo, frequentemente só o fundo transparente habilita a mistura, mas BG2 já cobre a linha. A R5 verifica os pixels visíveis antes de desenhar a subtela. Quando a mistura é necessária, usa paletas calculadas por cor secundária, sem refazer a aritmética de cada canal em cada pixel. Cores, prioridades, janelas, brilho, arredondamento de 5 bits e meia cor são preservados.

Paletas sem alterações mantêm os caches entre quadros. A comparação usa as cores originais, pois duas cores escuras podem produzir o mesmo RGB e resultados diferentes na mistura. Escritas de CGRAM, alterações diretas, brilho e wrap da geração de cache têm testes diferenciais.

| Caminho medido no computador | Tempo mediano de PPU R4 | R5 | Redução de tempo |
|---|---:|---:|---:|
| Castelo, subtela habilitada | 215,627 µs | 145,992 µs | 32,3% |
| Chuva | 290,742 µs | 214,415 µs | 26,3% |
| Escuridão com subtela de iluminação | 284,389 µs | 177,318 µs | 37,6% |
| Escuridão sem subtela | 129,166 µs | 127,943 µs | 0,9% |

São tempos de execução **no computador**, sem áudio e fora da apresentação SDL. Não são taxas de FPS nem previsões de desempenho do PS2. Castelo/chuva usam a mediana de três execuções por versão; escuridão usa uma comparação de 40 mil quadros, dos quais 2.463 têm a subtela de iluminação e 4.323 não têm. O fonte de teste usa GCC com sanitizer de alinhamento; o ELF usa a toolchain R5900.

## Contador opcional, sem menu

**L3** mostra ou oculta FPS e médias de tempo, atualizadas a cada segundo. `J` é lógica, `A` áudio, `P` PPU, `GS` envio/desenho, `V` espera de VBlank. Os tempos estão em milissegundos. O contador fica oculto por padrão e não altera o INI. Os timers por etapa não são consultados com o painel oculto. O painel não é uma ferramenta de perfil completa: seus próprios comandos de desenho entram no tempo GS.

A R5 mantém início direto, alvo 60 FPS, ausência de frameskip e a fila assíncrona de áudio da R4. O kernel DSP e os assets não foram alterados nesta revisão.

## Relação com o port do PSP

Referência analisada: [z2442/zelda3-PSP](https://github.com/z2442/zelda3-PSP), especialmente `src/main.c`, `src/psp_renderer.c` e `src/psp_audio_me.c`.

O PSP prepara geometria/tiles, apresenta com um VBlank e usa um processador auxiliar para o DSP. A R5 mantém a sincronização única e reduz o trabalho repetido de classificação de sprites; a transferência de áudio deixa de bloquear a apresentação. O código GU/GE e o Media Engine não podem ser ligados diretamente ao GS/IOP do PS2.

**A PPU não foi inteiramente migrada para hardware.** Tiles, composição, efeitos e Mode 7 ainda passam pela EE; o GS apresenta, escala e filtra. O DSP/SPC também continua na EE. A opção experimental de paletas foi desativada na inicialização desta revisão enquanto seu ganho no console não está medido.

Não há medição em PS2 nesta R5: a compilação e os testes não provam 60 FPS constantes nem identificam, sozinhos, o desempenho final das cenas relatadas. A alteração do limitador de apresentação é verificável no código; o desempenho final requer medição no console.

O código do menu e do seletor 30/60 continua arquivado no fonte para possível retomada, mas o menu não é ligado ao ELF R4 e o loop nativo não usa o seletor. Seus testes legados continuam executáveis.

Extrator de assets já entregue: https://zelda3-assets-extractor.agreeabledock1.chatgpt.site/ . Nenhuma alteração no site foi necessária nesta revisão.

## Compilar

Requer ps2dev completo, incluindo ps2sdk, gsKit, SDL2 e ps2_drivers. Exportar as variáveis do ambiente instalado:

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

## Verificações

[VALIDACAO.md](ps2/VALIDACAO.md) descreve os resultados e seus limites. Testes portáveis:

```sh
./ps2/host-test/run_unit_tests.sh
# Em ambientes que bloqueiam LeakSanitizer/ptrace, manter ASan/UBSan:
ASAN_OPTIONS=detect_leaks=0 ./ps2/host-test/run_unit_tests.sh
```

Para os replays, instale GCC e os arquivos de desenvolvimento SDL2. Compile também uma cópia original de referência, antes das alterações:

```sh
make -C ps2/host-test EXTRA=-DZ3_GS_TEST=1
python3 ps2/host-test/compare_replays.py /caminho/original/z3test ps2/host-test/z3test /caminho/zelda3_assets.dat /tmp/z3-argb
make -C ps2/host-test clean
make -C ps2/host-test EXTRA='-DZ3_GS_TEST=1 -DZ3_PIXEL_ABGR=1'
python3 ps2/host-test/compare_replays.py /caminho/original/z3test ps2/host-test/z3test /caminho/zelda3_assets.dat /tmp/z3-abgr
```
