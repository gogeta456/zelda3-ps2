# Validação — revisão 2 — 08/11/2026

A revisão anterior piorou o desempenho segundo o teste do usuário. Esta revisão muda o padrão para 30 FPS e corrige dois custos identificados no código. Os resultados abaixo são de compilação e testes no computador; ainda não existe medição de desempenho desta revisão em PS2/PCSX2.

## Alterações verificadas

- Padrão 30 FPS, 60 FPS opcional. Preferências `Version=1` migram para 30 FPS com paleta experimental desligada, preservando controles, som e SRAM. `Version=2` mantém novas escolhas salvas.
- Mesmo número de atualizações de lógica e áudio nos dois modos. Em 60.000 passos simulados, o modo 30 produz 30.000 desenhos e o modo 60 produz 60.000, ambos em 1.001.000 ms. Há verificação de wrap do contador de tempo e recuperação de atrasos.
- A recuperação antiga da fila de áudio bloqueou por 250 ms na simulação reproduzida. O novo consumidor IOP não avança sobre dados ausentes; os testes de falta de PCM voltam a enviar áudio com **0 ms de espera inserida**, sem reinicialização do formato. Isso não mede a latência física da SPU2.
- Testado o conversor real de amostras do código IOP: 341 amostras de entrada por 512 amostras de saída, em mono/estéreo. A cadência PCM da EE segue sua taxa efetiva, evitando acúmulo lento da fila. Não é uma validação sonora da introdução no console.
- Testes combinados de fila/cadência por 6.000 passos em 30 FPS estéreo, 60 FPS estéreo e 30 FPS mono; pausa/retomada, volume e falha de saída também verificados. O teste usa o mesmo código de política da fila com um relógio de SPU2 simulado.
- Política da fila IOP submetida a sequências de escrita/leitura, falta de dados, wrap e verificação dos bytes consumidos, para mono e estéreo. Um espaço de quatro bytes distingue cheia de vazia.
- Mantidos testes de 65.536 posições analógicas, mapeamentos, desconexão, menu, CRC, truncamento, SRAM alternada e escolha/remoção de memory card. Testada migração de preferências e manutenção da opção 60 FPS após salvar na revisão 2.
- Menu de três páginas dentro de 640×448; captura de teste inspecionada mostrando **30 FPS** e **paleta pelo GS: não** por padrão.
- **30 FPS:** introdução e 13 replays, 2.500 atualizações por caso = 35.000 atualizações de lógica; 17.500 imagens exibidas comparadas com as correspondentes da versão original, todas iguais.
- **60 FPS + paleta opcional:** os mesmos 35.000 passos, 35.000 imagens comparadas, todas iguais. Sem erros de alinhamento do UBSan nos dois percursos.
- Transferência RGBA reduzida de 256×256 para 256×224: 12,5% menos bytes por desenho. Retirada limpeza completa quando a imagem cobre a tela. Isso é uma redução de trabalho identificável no código, não um benchmark no GS.
- ELF recompilado e módulo IOP corrigido de 19.597 bytes conferido dentro dele, byte a byte, pelos símbolos `audsrv_irx` / `size_audsrv_irx`. Identificador do módulo: `0.93-z3r2`.
- ISO9660 nível 1 remontada; SYSTEM.CNF, ELF, assets e instruções extraídos novamente e conferidos byte a byte.

## Arquivos desta revisão

- ELF SHA-256: `35677c03820a8e40acd82c2e8e191b9a70af1738efa447af481fe7869c7f5caa`
- ISO SHA-256: `61a355099461510e0d33685a045c52487abc9f1eb9bc6116214ad765bab481dc`
- Assets mantidos: `0fe2e4bd75d70f06fb9a74cd3a9cb336c838149b831b56e8792114a89292c793`

Scripts, resultados e logs estão em `ps2/host-test/`, com resultados atuais em `results-r2/` e registros anteriores em `results-r1/`. O código e as licenças do módulo IOP estão em `ps2/iop/audsrv/`.

## Limites

A revisão 2 ainda não foi executada em PS2 nem em PCSX2 nesta sessão. Não foi confirmado o FPS sustentado no console, o som percebido na cena da Triforce nem o comportamento real de todos os modelos de controle e memory card. O GS é usado para apresentação, escala, filtro e menu, com paletas opcionais; composição PPU e síntese SPC/DSP continuam em parte na EE.

Os novos testes portáveis usam `-Wall -Wextra -Werror`, ASan e UBSan. LeakSanitizer foi desativado porque o ambiente não permite sua inspeção de processos. O assembly original do ícone provoca um aviso GNU-stack no linker host. O código herdado do jogo conserva avisos de compilação. Os replays de imagens não reproduzem áudio real e não são benchmarks.
