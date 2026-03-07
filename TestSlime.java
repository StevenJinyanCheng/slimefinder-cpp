import java.util.Random;

public class TestSlime {
    public static boolean isSlimeChunk(long seed, int chunkX, int chunkZ) {
        Random r = new Random(
            seed +
            (long) (chunkX * chunkX * 4987142) +
            (long) (chunkX * 5947611) +
            (long) (chunkZ * chunkZ) * 4392871L +
            (long) (chunkZ * 389711) ^ 987234911L
        );
        return r.nextInt(10) == 0;
    }
    public static void main(String[] args) {
        long seed = 0;
        int count = 0;
        for (int dx = -8; dx <= 8; dx++) {
            for (int dz = -8; dz <= 8; dz++) {
                if(isSlimeChunk(seed, 88 + dx, -797 + dz)) count++;
            }
        }
        System.out.println("Slime chunks in 17x17 around 88,-797: " + count);
    }
}
