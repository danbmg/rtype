# Server weapons and bonuses

The server owns shooting, drops, collection and effect duration. Clients keep sending FIRE;
they cannot grant bonuses or select a weapon through a network command.

## Shared components

`common/components/Equipment.hpp` defines `Equipment` (weapon and rapid-fire timer),
`Pickup` (collectible type and lifetime), and `LootDrop` (an enemy's assigned drop).

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

Weapon pickups replace the current weapon. Rapid fire works with every weapon and collecting
it again refreshes its timer to 8 seconds. Losing a life resets the weapon to Standard and
cancels rapid fire. Level changes preserve equipment; timers continue during pauses.

## Drops and collection

`WaveDefinition::pickupDropChance` is a probability in [0, 1], defaulting to 0.2. Bosses always
receive loot. The level director uses the campaign's seeded RNG to assign drops at enemy
spawn, choosing uniformly among the six types. Death cleanup emits the item at the enemy's
last position, including deaths from contact. Escaping enemies never drop loot.

Items have a 20 × 20 hitbox, drift left at 45 world units/s, and expire after 12 seconds or
when they leave the screen. The first overlapping living player collects an item, which is
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

Protocol **2** introduces item entity kinds 6–11 and appends `weapon` (u8) and
`rapidFireMilliseconds` (u16) to `PlayerState`, now 10 bytes. Existing lives and health fields
report healing and extra lives. Normal entity packets replicate item positions and removals.
See the [protocol RFC](protocol/rfc.md).

Rebuild both client and server. The network client retains the decoded equipment fields;
the graphical client can use them for collectibles and its HUD. This change adds no pickup
textures, drawings or equipment HUD. Every weapon uses the existing player-projectile entity
kind, so dedicated projectile visuals can be added separately.
