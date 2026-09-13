# PC real light graph: index, save references and fresh native load

After original FAT entry cleanup, a declared two-entry directory is built
from the saved entries' actual ID/class/offset/size, then parsed by actual
466B90. The saved references are consumed unchanged through whole4678B0,
including actual specialized DXLight header creation, nested Node loading,
owning attachment and world propagation. The repeated root resolves to the
same newly loaded object; null consumes exactly four bytes. Both original
and fresh graphs are destroyed by native destructors, plus all helper owners.
