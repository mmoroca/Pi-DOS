#ifndef PIDOSCFG_MIDI_H
#define PIDOSCFG_MIDI_H
#define MIDI_PORT_LIMIT 32
extern char midi_port_ids[MIDI_PORT_LIMIT][23],midi_port_labels[MIDI_PORT_LIMIT][80];
int midi_scan(void);
int midi_ports(void);
#endif
