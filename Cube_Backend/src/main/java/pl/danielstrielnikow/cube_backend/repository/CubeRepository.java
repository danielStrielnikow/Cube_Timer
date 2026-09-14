package pl.danielstrielnikow.cube_backend.repository;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;
import pl.danielstrielnikow.cube_backend.model.Cube;

import java.util.Optional;

@Repository
public interface CubeRepository extends JpaRepository<Cube, Long> {
    Optional<Cube> findBySerialNumber(String serialNumber);
}
