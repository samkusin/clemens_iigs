
1. ROM loaded into memory
2. clemens_host_setup()
3. clemens_init()
4. Restore BRAM memory from storage
5. clemens_assign_audio_mix_buffer()


## Gotchas

1. Only one machine can run at a time
   1. The machine is *mostly* self contained within `ClemensMachine`
   2. Logging currently requires a global machine context
   3. For now, if you need to run multiple machines at the same time, you'll have to mutex calls to `clemens_emulate()`
   4. This limitation is annoying
   5. It should be possible with some work to remove this limitation in a later release
