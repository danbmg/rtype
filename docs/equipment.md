# Server weapons and bonuses

The server owns shooting, drops, collection and effect duration. Clients keep sending FIRE;
they cannot grant bonuses or select a weapon through a network command.

## Shared components

`common/components/Equipment.hpp` defines `Equipment` (weapon, rapid-fire, shield and
invulnerability timers), `Pickup` (collectible type and lifetime), and `LootDrop` (an enemy's
assigned drop).

| Weapon | Projectiles per volley | Damage per projectile | Cooldown |
| --- | --- | --- | --- |
| Standard | 1 straight | 1 | 0.25 s |
| Double | 2 parallel, vertically offset | 1 | 0.25 s |
| Spread | 3 at angles -0.25, 0 and +0.25 radians | 1 | 0.25 s |
| Heavy | 1 straight | 3 | 0.5 s |

Cooldowns are quantized to at least one simulation tick. Projectiles retain their shooter's
player ID and use the existing collisions and scoring. Heavy shots do not pierce targets.

| Pickup | Effect |
| --- | --- |
| ExtraLife | +1 life, capped at 9 |
| Heal | Restore current life to maximum HP |
| RapidFire | Multiply weapon cooldown by 0.4 for 8 seconds |
| DoubleShot | Equip Double |
| SpreadShot | Equip Spread |
| HeavyShot | Equip Heavy |
| Shield | Absorb the next enemy shot or collision; lasts 12 seconds at most |
| Trophy | The Summoner's Cup: +10000 points; catching it completes the story (see [game design](game-design.md)) |

Weapon pickups replace the current weapon. Rapid fire works with every weapon and collecting
it again refreshes its timer to 8 seconds. Losing a life resets the weapon to Standard and
cancels rapid fire and the shield. After losing a life, a ship is invulnerable for 2 seconds:
enemy shots fly through it and collisions do nothing. Level changes preserve equipment; timers
continue during pauses.

## Drops and collection

`WaveDefinition::pickupDropChance` is a probability in [0, 1], defaulting to 0.2. Bosses always
receive loot. The level director uses the campaign's seeded RNG to assign drops at enemy
spawn, choosing uniformly among the seven regular types (extra life to shield). The final
boss of a finite campaign drops the Trophy instead; in endless mode it drops a regular item. Death cleanup emits the item at the enemy's
last position, including deaths from contact. Escaping enemies never drop loot.

Items have a 20 × 20 hitbox, drift left at 45 world units/s, and expire after 12 seconds or
when they leave the screen. The Trophy is different: 40 × 40, it stays where the boss fell and
never expires, and the level is not cleared until a player catches it. The first overlapping living player collects an item, which is
removed immediately. Dead ships cannot collect. Items are consumed even at full health or
the lives cap.

Drops do not block wave completion. They persist across automatic wave/level transitions
until collected or expired. Explicit `GameWorld::loadLevel` clears them alongside enemies
and projectiles, preserving player equipment.

## Gameplay API

Implemented in `game/EquipmentSystems.hpp` and `.cpp`:

```cpp
// Spawn a collectible, or grant a bonus through trusted server code.
game::spawnPickup(registry,
    rtype::common::components::PickupType::RapidFire, 400.0F, 200.0F);
game::applyPickup(registry, ship,
    rtype::common::components::PickupType::SpreadShot);
```

- `fireEquippedWeapon`: create a volley.
- `weaponCooldownSeconds`: obtain the effective cooldown for server rate limiting.
- `updateEquipment`: advance effects and item lifetimes.
- `collectPickups`: apply overlap-based collection once per item.

`registerGameplaySystems` registers timers before movement and collection after damage
collisions, before player deaths. New drops can be collected on the following tick.
`GameWorld::applyInput` enforces the cooldown even with repeated FIRE packets in one tick.

## Network and client integration

Protocol **2** introduced item entity kinds 6–11 and the `weapon` and `rapidFireMilliseconds`
fields of `PlayerState`. Protocol **3** adds the Shield (12) and Trophy (13) kinds and a `flags`
byte (shield up, invulnerable), so `PlayerState` is 11 bytes. Normal entity packets replicate
item positions and removals. See the [protocol RFC](protocol/rfc.md).

The graphical client draws each item as a capsule with a letter (`assets/pickups.png`), the
Cup with a glow and a "catch the Cup" label, a bubble around shielded ships and a blinking ship
while invulnerable. Its HUD shows the weapon, the shield and a rapid-fire bar per player.
