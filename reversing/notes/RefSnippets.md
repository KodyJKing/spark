Get entity from handle:
```C
  entity = (Entity *)
           (DATA_EntityPool +
           (longlong)
           *(int *)(DATA_EntityRecordPool->name +
                   (ulonglong)((uint)entityHandle & 0xffff) * 0xc +
                   (longlong)DATA_EntityRecordPool->objectsOffset + 8) + 0x34);
```